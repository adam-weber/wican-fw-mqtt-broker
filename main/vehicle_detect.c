/*
 * This file is part of the WiCAN project.
 *
 * Copyright (C) 2022  Meatpi Electronics.
 * Written by Ali Slim <ali@meatpi.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "cJSON.h"
#include "vehicle_detect.h"
#include "can.h"

static const char *TAG = "vehicle_detect";

#define MAX_CAN_ADDRESSES 512
#define MAX_FINGERPRINTS 100

// Global state
static struct {
    vehicle_fingerprint_t fingerprints[MAX_FINGERPRINTS];
    uint16_t fingerprint_count;

    can_address_entry_t seen_addresses[MAX_CAN_ADDRESSES];
    uint16_t seen_count;

    detection_result_t current_result;
    detection_status_t current_status;

    TaskHandle_t detection_task;
    SemaphoreHandle_t mutex;

    bool initialized;
} vd_state = {0};

// Forward declarations
static int scan_can_bus(uint32_t duration_ms);
static int match_fingerprints(detection_result_t *result);
static float calculate_confidence(const vehicle_fingerprint_t *fp,
                                  const can_address_entry_t *seen,
                                  uint16_t seen_count,
                                  uint8_t *matched);
static bool is_address_seen(uint32_t address);
static void detection_task(void *pvParameters);
static int compare_matches(const void *a, const void *b);

int vehicle_detect_init(void)
{
    if (vd_state.initialized) {
        ESP_LOGW(TAG, "Already initialized");
        return 0;
    }

    vd_state.mutex = xSemaphoreCreateMutex();
    if (vd_state.mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create mutex");
        return -1;
    }

    vd_state.fingerprint_count = 0;
    vd_state.seen_count = 0;
    vd_state.detection_task = NULL;
    vd_state.initialized = true;

    ESP_LOGI(TAG, "Vehicle detection initialized");
    return 0;
}

int vehicle_detect_start(detection_result_t *result)
{
    if (!vd_state.initialized) {
        ESP_LOGE(TAG, "Not initialized");
        return -1;
    }

    if (!can_is_enabled()) {
        ESP_LOGE(TAG, "CAN bus not enabled");
        return -2;
    }

    if (vd_state.fingerprint_count == 0) {
        ESP_LOGW(TAG, "No fingerprints loaded");
        return -3;
    }

    ESP_LOGI(TAG, "Starting vehicle detection (blocking mode)");

    xSemaphoreTake(vd_state.mutex, portMAX_DELAY);

    // Reset state
    memset(&vd_state.current_result, 0, sizeof(detection_result_t));
    memset(&vd_state.current_status, 0, sizeof(detection_status_t));
    vd_state.seen_count = 0;
    vd_state.current_status.in_progress = true;

    uint32_t start_time = esp_timer_get_time() / 1000;

    // Scan CAN bus
    int ret = scan_can_bus(DETECTION_TIMEOUT_MS);
    if (ret < 0) {
        ESP_LOGE(TAG, "CAN bus scan failed");
        vd_state.current_status.in_progress = false;
        xSemaphoreGive(vd_state.mutex);
        return ret;
    }

    uint32_t end_time = esp_timer_get_time() / 1000;
    vd_state.current_result.scan_duration_ms = end_time - start_time;
    vd_state.current_result.total_addresses_seen = vd_state.seen_count;

    ESP_LOGI(TAG, "Scan complete: %d addresses in %lu ms",
             vd_state.seen_count, vd_state.current_result.scan_duration_ms);

    // Match against fingerprints
    ret = match_fingerprints(&vd_state.current_result);
    if (ret < 0) {
        ESP_LOGE(TAG, "Fingerprint matching failed");
        vd_state.current_status.in_progress = false;
        xSemaphoreGive(vd_state.mutex);
        return ret;
    }

    vd_state.current_result.detection_complete = true;
    vd_state.current_status.in_progress = false;
    vd_state.current_status.progress_percent = 100;

    // Copy result
    if (result) {
        memcpy(result, &vd_state.current_result, sizeof(detection_result_t));
    }

    xSemaphoreGive(vd_state.mutex);

    ESP_LOGI(TAG, "Detection complete: %d matches found",
             vd_state.current_result.match_count);

    return 0;
}

int vehicle_detect_start_async(void)
{
    if (!vd_state.initialized) {
        ESP_LOGE(TAG, "Not initialized");
        return -1;
    }

    if (vd_state.detection_task != NULL) {
        ESP_LOGW(TAG, "Detection already in progress");
        return -2;
    }

    BaseType_t ret = xTaskCreate(
        detection_task,
        "vehicle_detect",
        4096,
        NULL,
        5,
        &vd_state.detection_task
    );

    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create detection task");
        return -3;
    }

    ESP_LOGI(TAG, "Async detection started");
    return 0;
}

int vehicle_detect_get_status(detection_status_t *status)
{
    if (!vd_state.initialized) {
        return -1;
    }

    xSemaphoreTake(vd_state.mutex, portMAX_DELAY);
    memcpy(status, &vd_state.current_status, sizeof(detection_status_t));
    xSemaphoreGive(vd_state.mutex);

    return 0;
}

int vehicle_detect_get_result(detection_result_t *result)
{
    if (!vd_state.initialized) {
        return -1;
    }

    xSemaphoreTake(vd_state.mutex, portMAX_DELAY);

    if (!vd_state.current_result.detection_complete) {
        xSemaphoreGive(vd_state.mutex);
        return -1;  // Not complete
    }

    memcpy(result, &vd_state.current_result, sizeof(detection_result_t));
    xSemaphoreGive(vd_state.mutex);

    return 0;
}

void vehicle_detect_stop(void)
{
    if (vd_state.detection_task != NULL) {
        vTaskDelete(vd_state.detection_task);
        vd_state.detection_task = NULL;
        vd_state.current_status.in_progress = false;
        ESP_LOGI(TAG, "Detection stopped");
    }
}

int vehicle_detect_load_fingerprints(const char *json_data)
{
    if (!vd_state.initialized) {
        ESP_LOGE(TAG, "Not initialized");
        return -1;
    }

    cJSON *root = cJSON_Parse(json_data);
    if (root == NULL) {
        ESP_LOGE(TAG, "Failed to parse JSON");
        return -2;
    }

    cJSON *vehicles = cJSON_GetObjectItem(root, "vehicles");
    if (!cJSON_IsArray(vehicles)) {
        ESP_LOGE(TAG, "Invalid fingerprint format: missing 'vehicles' array");
        cJSON_Delete(root);
        return -3;
    }

    xSemaphoreTake(vd_state.mutex, portMAX_DELAY);
    vd_state.fingerprint_count = 0;

    cJSON *vehicle = NULL;
    cJSON_ArrayForEach(vehicle, vehicles) {
        if (vd_state.fingerprint_count >= MAX_FINGERPRINTS) {
            ESP_LOGW(TAG, "Max fingerprints reached, stopping");
            break;
        }

        cJSON *name = cJSON_GetObjectItem(vehicle, "name");
        cJSON *fingerprint = cJSON_GetObjectItem(vehicle, "fingerprint");

        if (!name || !fingerprint) {
            ESP_LOGW(TAG, "Skipping invalid vehicle entry");
            continue;
        }

        vehicle_fingerprint_t *fp = &vd_state.fingerprints[vd_state.fingerprint_count];
        memset(fp, 0, sizeof(vehicle_fingerprint_t));

        // Vehicle name
        strncpy(fp->vehicle_name, name->valuestring, MAX_VEHICLE_NAME_LEN - 1);

        // Year range (optional)
        cJSON *year = cJSON_GetObjectItem(vehicle, "year");
        if (year && cJSON_IsString(year)) {
            strncpy(fp->year_range, year->valuestring, sizeof(fp->year_range) - 1);
        }

        // Required addresses
        cJSON *required = cJSON_GetObjectItem(fingerprint, "required_addresses");
        if (cJSON_IsArray(required)) {
            cJSON *addr = NULL;
            uint8_t count = 0;
            cJSON_ArrayForEach(addr, required) {
                if (count >= MAX_FINGERPRINT_ADDRESSES) break;
                if (cJSON_IsNumber(addr)) {
                    fp->required_addresses[count++] = (uint32_t)addr->valueint;
                }
            }
            fp->required_count = count;
        }

        // Min match count
        cJSON *min_match = cJSON_GetObjectItem(fingerprint, "min_match_count");
        if (cJSON_IsNumber(min_match)) {
            fp->min_match_count = (uint8_t)min_match->valueint;
        } else {
            // Default to 80% of required addresses
            fp->min_match_count = (fp->required_count * 8) / 10;
        }

        // VIN pattern (optional)
        cJSON *vin = cJSON_GetObjectItem(fingerprint, "vin_pattern");
        if (vin && cJSON_IsString(vin)) {
            strncpy(fp->vin_pattern, vin->valuestring, sizeof(fp->vin_pattern) - 1);
        }

        ESP_LOGI(TAG, "Loaded fingerprint: %s (%s) - %d addresses",
                 fp->vehicle_name, fp->year_range, fp->required_count);

        vd_state.fingerprint_count++;
    }

    xSemaphoreGive(vd_state.mutex);
    cJSON_Delete(root);

    ESP_LOGI(TAG, "Loaded %d vehicle fingerprints", vd_state.fingerprint_count);
    return vd_state.fingerprint_count;
}

int vehicle_detect_get_fingerprint_count(void)
{
    return vd_state.fingerprint_count;
}

int vehicle_detect_learn_fingerprint(const char *vehicle_name, uint32_t duration_ms)
{
    if (!vd_state.initialized || !can_is_enabled()) {
        return -1;
    }

    ESP_LOGI(TAG, "Learning fingerprint for: %s", vehicle_name);

    xSemaphoreTake(vd_state.mutex, portMAX_DELAY);
    vd_state.seen_count = 0;

    int ret = scan_can_bus(duration_ms);

    if (ret == 0 && vd_state.seen_count > 0) {
        ESP_LOGI(TAG, "Learned %d addresses for %s", vd_state.seen_count, vehicle_name);

        // Here you could save this to a file or send to server
        // For now, just log the addresses
        ESP_LOGI(TAG, "Addresses seen:");
        for (uint16_t i = 0; i < vd_state.seen_count; i++) {
            ESP_LOGI(TAG, "  0x%03lX (dlc=%d, count=%lu)",
                     vd_state.seen_addresses[i].address,
                     vd_state.seen_addresses[i].dlc,
                     vd_state.seen_addresses[i].msg_count);
        }
    }

    xSemaphoreGive(vd_state.mutex);
    return ret;
}

// Internal functions

static int scan_can_bus(uint32_t duration_ms)
{
    uint32_t start_ms = esp_timer_get_time() / 1000;
    uint32_t current_ms;
    twai_message_t rx_msg;

    ESP_LOGI(TAG, "Scanning CAN bus for %lu ms...", duration_ms);

    vd_state.seen_count = 0;

    do {
        current_ms = esp_timer_get_time() / 1000;
        vd_state.current_status.elapsed_ms = current_ms - start_ms;
        vd_state.current_status.progress_percent =
            (vd_state.current_status.elapsed_ms * 100) / duration_ms;
        vd_state.current_status.addresses_seen = vd_state.seen_count;

        // Try to receive CAN message
        esp_err_t ret = can_receive(&rx_msg, pdMS_TO_TICKS(100));

        if (ret == ESP_OK) {
            uint32_t addr = rx_msg.identifier;

            // Check if we've seen this address before
            int idx = -1;
            for (uint16_t i = 0; i < vd_state.seen_count; i++) {
                if (vd_state.seen_addresses[i].address == addr) {
                    idx = i;
                    break;
                }
            }

            if (idx >= 0) {
                // Update existing entry
                vd_state.seen_addresses[idx].last_seen_ms = current_ms;
                vd_state.seen_addresses[idx].msg_count++;
            } else {
                // New address
                if (vd_state.seen_count < MAX_CAN_ADDRESSES) {
                    can_address_entry_t *entry = &vd_state.seen_addresses[vd_state.seen_count];
                    entry->address = addr;
                    entry->dlc = rx_msg.data_length_code;
                    entry->first_seen_ms = current_ms;
                    entry->last_seen_ms = current_ms;
                    entry->msg_count = 1;
                    vd_state.seen_count++;
                }
            }
        }

    } while ((current_ms - start_ms) < duration_ms);

    ESP_LOGI(TAG, "Scan complete: found %d unique addresses", vd_state.seen_count);
    return 0;
}

static int match_fingerprints(detection_result_t *result)
{
    result->match_count = 0;

    for (uint16_t i = 0; i < vd_state.fingerprint_count; i++) {
        if (result->match_count >= MAX_VEHICLE_MATCHES) {
            break;
        }

        vehicle_fingerprint_t *fp = &vd_state.fingerprints[i];
        uint8_t matched = 0;

        float confidence = calculate_confidence(
            fp,
            vd_state.seen_addresses,
            vd_state.seen_count,
            &matched
        );

        // Only include if meets minimum match threshold
        if (matched >= fp->min_match_count && confidence > 0.5f) {
            vehicle_match_t *match = &result->matches[result->match_count];

            strncpy(match->vehicle_name, fp->vehicle_name, MAX_VEHICLE_NAME_LEN - 1);
            strncpy(match->year_range, fp->year_range, sizeof(match->year_range) - 1);
            match->confidence = confidence;
            match->matched_addresses = matched;
            match->total_required = fp->required_count;

            result->match_count++;
        }
    }

    // Sort matches by confidence (highest first)
    if (result->match_count > 1) {
        qsort(result->matches, result->match_count,
              sizeof(vehicle_match_t), compare_matches);
    }

    return 0;
}

static float calculate_confidence(const vehicle_fingerprint_t *fp,
                                  const can_address_entry_t *seen,
                                  uint16_t seen_count,
                                  uint8_t *matched)
{
    *matched = 0;

    if (fp->required_count == 0) {
        return 0.0f;
    }

    // Count how many required addresses we've seen
    for (uint8_t i = 0; i < fp->required_count; i++) {
        if (is_address_seen(fp->required_addresses[i])) {
            (*matched)++;
        }
    }

    // Simple confidence: percentage of required addresses matched
    float confidence = (float)(*matched) / (float)fp->required_count;

    // Bonus points if we matched with few extra addresses
    // (indicates a clean match vs noise)
    if (confidence > 0.7f) {
        uint16_t extra_addresses = seen_count > (*matched) ?
                                   seen_count - (*matched) : 0;

        // Penalize if we see way more addresses than expected
        if (extra_addresses > fp->required_count) {
            confidence *= 0.95f;
        }
    }

    return confidence > 1.0f ? 1.0f : confidence;
}

static bool is_address_seen(uint32_t address)
{
    for (uint16_t i = 0; i < vd_state.seen_count; i++) {
        if (vd_state.seen_addresses[i].address == address) {
            return true;
        }
    }
    return false;
}

static void detection_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Detection task started");

    vehicle_detect_start(&vd_state.current_result);

    vd_state.detection_task = NULL;
    vTaskDelete(NULL);
}

static int compare_matches(const void *a, const void *b)
{
    const vehicle_match_t *match_a = (const vehicle_match_t *)a;
    const vehicle_match_t *match_b = (const vehicle_match_t *)b;

    // Sort descending by confidence
    if (match_a->confidence > match_b->confidence) return -1;
    if (match_a->confidence < match_b->confidence) return 1;
    return 0;
}
