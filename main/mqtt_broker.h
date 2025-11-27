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

#ifndef __MQTT_BROKER_H__
#define __MQTT_BROKER_H__

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Initialize and start the MQTT broker
 *
 * @param port The port number to listen on (default 1883)
 * @return 0 on success, -1 on failure
 */
int mqtt_broker_init(uint16_t port);

/**
 * @brief Stop the MQTT broker
 */
void mqtt_broker_stop(void);

/**
 * @brief Check if the MQTT broker is running
 *
 * @return true if running, false otherwise
 */
bool mqtt_broker_is_running(void);

/**
 * @brief Get the number of connected clients
 *
 * @return Number of connected clients
 */
uint8_t mqtt_broker_get_client_count(void);

#endif /* __MQTT_BROKER_H__ */
