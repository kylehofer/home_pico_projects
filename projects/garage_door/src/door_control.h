/*
 * Copyright (c) 2021 Kyle Hofer
 * Email: kylehofer@neurak.com.au
 * Web: https://neurak.com.au/
 *
 * This file is part of GarageDoor.
 *
 * GarageDoor is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * GarageDoor is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef DOOR_CONTROL
#define DOOR_CONTROL

#include <stdint.h>

// Pin Configurations
#define RELAY_PIN 22
#define POTENTIOMETER_PIN 26
#define DOOR_DATA_READY 0
#define DOOR_DATA_RECEIVED 1

enum door_state
{
    ACTIVE,
    MONITOR,
    START,
    STOP,
    DOOR_IDLE
};

enum door_result
{
    NONE,
    SUCCESS,
    FAIL
};

#define DOOR_DATA_SIZE (sizeof(uint8_t) + sizeof(uint8_t) + sizeof(uint8_t) + sizeof(uint8_t))

typedef struct _DoorControl
{
    enum door_state current_state;
    enum door_state desired_state;
    enum door_result result;
    uint16_t current_position;
    uint16_t desired_position;
    int16_t position_min;
    int16_t position_max;
    int8_t mean_position_delta;
    uint8_t relay_output;
    uint32_t time_stamp;
    uint32_t command_time;
    uint32_t relay_time;
    uint8_t stablize_count;
    uint8_t hold_transition;
} DoorControl;

typedef struct _DoorData
{
    uint8_t position;
    uint8_t state;
    uint8_t result;
} DoorData;

#ifdef __cplusplus
extern "C"
{
#endif

    void door_control_set_position(DoorControl *door_control, uint8_t position);
    int door_control_execute(DoorControl *door_control);
    DoorControl door_control_initialize(DoorControl *door_control, int16_t min, int16_t max);
    uint16_t door_control_calibrate_min_position(DoorControl *door_control);
    uint16_t door_control_calibrate_max_position(DoorControl *door_control);
    DoorData door_control_get(DoorControl *door_control);

    inline long linear_map(long value, long in_min, long in_max, long out_min, long out_max)
    {
        return (value - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
    }

#ifdef __cplusplus
}
#endif

#endif /* DOOR_CONTROL */
