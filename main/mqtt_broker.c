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
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "mosq_broker.h"
#include "mqtt_broker.h"

static const char *TAG = "mqtt_broker";

static TaskHandle_t broker_task_handle = NULL;
static bool broker_running = false;
static uint16_t broker_port = 1883;

/**
 * @brief MQTT Broker task
 */
static void mqtt_broker_task(void *pvParameters)
{
    struct mosq_broker_config config = {0};

    // Configure broker to listen on all interfaces
    config.host = "0.0.0.0";
    config.port = broker_port;
    config.tls_cfg = NULL;  // No TLS for now (can be added later)

    ESP_LOGI(TAG, "Starting MQTT broker on port %d", broker_port);

    broker_running = true;

    // Run the broker (this blocks)
    int ret = mosq_broker_run(&config);

    if (ret != 0) {
        ESP_LOGE(TAG, "Broker failed to start or stopped with error: %d", ret);
    } else {
        ESP_LOGI(TAG, "Broker stopped normally");
    }

    broker_running = false;
    broker_task_handle = NULL;

    vTaskDelete(NULL);
}

int mqtt_broker_init(uint16_t port)
{
    if (broker_task_handle != NULL) {
        ESP_LOGW(TAG, "Broker already running");
        return -1;
    }

    broker_port = port;

    // Create the broker task with sufficient stack (6KB as recommended by Espressif)
    BaseType_t result = xTaskCreate(
        mqtt_broker_task,
        "mqtt_broker",
        6144,  // 6KB stack size
        NULL,
        5,     // Priority 5 (same as other network tasks)
        &broker_task_handle
    );

    if (result != pdPASS) {
        ESP_LOGE(TAG, "Failed to create broker task");
        return -1;
    }

    ESP_LOGI(TAG, "MQTT broker task created successfully");
    return 0;
}

void mqtt_broker_stop(void)
{
    if (broker_task_handle != NULL) {
        ESP_LOGI(TAG, "Stopping MQTT broker");

        // Note: mosq_broker_run() doesn't have a clean shutdown API
        // We'll need to delete the task (not ideal, but functional)
        vTaskDelete(broker_task_handle);
        broker_task_handle = NULL;
        broker_running = false;

        ESP_LOGI(TAG, "MQTT broker stopped");
    }
}

bool mqtt_broker_is_running(void)
{
    return broker_running;
}

uint8_t mqtt_broker_get_client_count(void)
{
    // TODO: Implement client count tracking
    // This would require modifications to the mosquitto broker library
    // or maintaining our own client list
    return 0;
}
