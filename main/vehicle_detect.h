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

#ifndef __VEHICLE_DETECT_H__
#define __VEHICLE_DETECT_H__

#include <stdint.h>
#include <stdbool.h>

#define MAX_FINGERPRINT_ADDRESSES 32
#define MAX_VEHICLE_NAME_LEN 64
#define MAX_VEHICLE_MATCHES 10
#define DETECTION_TIMEOUT_MS 15000  // 15 seconds scan time

/**
 * @brief CAN address entry seen during fingerprinting
 */
typedef struct {
    uint32_t address;
    uint8_t dlc;
    uint32_t first_seen_ms;
    uint32_t last_seen_ms;
    uint32_t msg_count;
} can_address_entry_t;

/**
 * @brief Vehicle fingerprint definition
 */
typedef struct {
    char vehicle_name[MAX_VEHICLE_NAME_LEN];
    char year_range[16];  // e.g., "2018-2023"
    uint32_t required_addresses[MAX_FINGERPRINT_ADDRESSES];
    uint8_t required_count;
    uint8_t min_match_count;  // Minimum matches needed for positive ID
    char vin_pattern[16];     // VIN prefix pattern (optional)
} vehicle_fingerprint_t;

/**
 * @brief Vehicle match result
 */
typedef struct {
    char vehicle_name[MAX_VEHICLE_NAME_LEN];
    char year_range[16];
    float confidence;  // 0.0 - 1.0
    uint8_t matched_addresses;
    uint8_t total_required;
} vehicle_match_t;

/**
 * @brief Detection result containing all matches
 */
typedef struct {
    vehicle_match_t matches[MAX_VEHICLE_MATCHES];
    uint8_t match_count;
    uint32_t total_addresses_seen;
    uint32_t scan_duration_ms;
    bool detection_complete;
} detection_result_t;

/**
 * @brief Detection status for progress reporting
 */
typedef struct {
    bool in_progress;
    uint32_t elapsed_ms;
    uint32_t addresses_seen;
    uint8_t progress_percent;  // 0-100
} detection_status_t;

/**
 * @brief Initialize vehicle detection subsystem
 *
 * @return 0 on success, negative on error
 */
int vehicle_detect_init(void);

/**
 * @brief Start vehicle fingerprint detection
 *
 * Performs passive CAN bus listening to identify vehicle.
 * This is a blocking operation that takes ~15 seconds.
 *
 * @param result Pointer to store detection results
 * @return 0 on success, negative on error
 */
int vehicle_detect_start(detection_result_t *result);

/**
 * @brief Start vehicle detection in non-blocking mode
 *
 * Launches detection in a separate task.
 * Use vehicle_detect_get_status() to monitor progress.
 *
 * @return 0 on success, negative on error
 */
int vehicle_detect_start_async(void);

/**
 * @brief Get current detection status
 *
 * @param status Pointer to store current status
 * @return 0 on success, negative on error
 */
int vehicle_detect_get_status(detection_status_t *status);

/**
 * @brief Get results from async detection
 *
 * @param result Pointer to store detection results
 * @return 0 on success, -1 if detection not complete, negative on error
 */
int vehicle_detect_get_result(detection_result_t *result);

/**
 * @brief Stop ongoing detection
 */
void vehicle_detect_stop(void);

/**
 * @brief Load vehicle fingerprints from JSON file
 *
 * @param json_data JSON string containing fingerprint definitions
 * @return Number of fingerprints loaded, negative on error
 */
int vehicle_detect_load_fingerprints(const char *json_data);

/**
 * @brief Get number of loaded vehicle fingerprints
 *
 * @return Number of fingerprints in database
 */
int vehicle_detect_get_fingerprint_count(void);

/**
 * @brief Manual fingerprint: store current CAN addresses for learning
 *
 * This allows users to contribute unknown vehicle fingerprints.
 *
 * @param vehicle_name User-provided vehicle name
 * @param duration_ms How long to scan (milliseconds)
 * @return 0 on success, negative on error
 */
int vehicle_detect_learn_fingerprint(const char *vehicle_name, uint32_t duration_ms);

#endif // __VEHICLE_DETECT_H__
