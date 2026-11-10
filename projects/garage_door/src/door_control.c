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
/*
 * This file is for controlling a garage door.
 * The position of the door is read from a potentiometer, and door control is handled by using a relay.
 * Control of this garage door is very primitive as it can only be controlled by using a single push button switch.
 * The door reacts to the button press in the following ways:
 * * If the door is not moving - Move in the opposite direction of the previous movement
 * * If the door is moving - Stop moving
 *
 * The door control will use the relay to move the door if required, and will read the values from the potentiometer to calculate the direction of the movement.
 * If the direction is opposite of what is expected, the door control will stop, then restart the door.
 * If the door moves unexpectingly, the door control will release control and let the door control resume back to normal.
 * This last function is for safety as the door has force controls to prevent damage to the door or any objects caught in its way.
 */

#include "door_control.h"
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include <stdio.h>

// Timers
#define RELAY_DELAY 100000
#define DOOR_SAMPLE_RATE 35000
#define DOOR_COMMAND_DELAY 1250000
#define STABLIZE_COUNT_MAX 5

// Thresholds
#define DELTA_THRESHOLD 8
#define DOOR_THRESHOLD 90

#define MAX_DOOR_VALUE 0xFFFF

#define MAX_TIME 0xFFFFFFFF

#define LOW 0
#define HIGH 1

bool timestamp_elapsed(uint32_t stamp, uint32_t max)
{
    uint32_t now = time_us_32();
    return (now > stamp && ((MAX_TIME - now) + stamp) > max);
}

// Union used for converting door data to a buffer for transmissions
typedef union
{
    uint8_t buffer[DOOR_DATA_SIZE];
    struct
    {
        uint8_t identifier;
        uint8_t position;
        uint8_t state;
        uint8_t result;
    };
} DoorToBuffer_u;

/**
 * Initializes the door control component and sets up pins for controlling the relay.
 * @returns a garage door control struct
 */
DoorControl door_control_initialize(DoorControl *door_control, int16_t min, int16_t max)
{
    gpio_init(RELAY_PIN);
    gpio_set_dir(RELAY_PIN, GPIO_OUT);

    door_control->time_stamp = time_us_32();

    *door_control = (DoorControl){
        .current_state = DOOR_IDLE,
        .desired_state = DOOR_IDLE,
        .result = NONE,
        .current_position = 0,
        .desired_position = 0,
        .position_closed = min,
        .position_open = max,
        .mean_position_delta = 0,
        .relay_output = 0,
        .time_stamp = 0,
        .command_time = 0,
        .relay_time = 0,
        .stablize_count = 0,
        .hold_transition = 0};

    return *door_control;
}

/**
 * Reads data from the door control and puts it into a buffer for sending over serial
 *
 * @param door_control a pointer to the door control struct
 * @param data a pointer to the buffer to transfer data to
 * @return The number of bytes added to the buffer
 */
DoorData door_control_get(DoorControl *door_control)
{
    DoorToBuffer_u door_data;

    return (DoorData){
        .position = linear_map(
            door_control->current_position,
            door_control->position_closed,
            door_control->position_open,
            0,
            100),
        .state = (uint8_t)door_control->current_state,
        .result = (uint8_t)door_control->result};
}

/**
 * @brief Trigger the door control relay
 *
 * @param door_control a pointer to the door control struct
 */
void door_control_trigger(DoorControl *door_control)
{
    door_control->desired_state = TRIGGER;
    door_control->result = NONE;
}

/**
 * Gets the current position for the garage door.
 * @param door_control a pointer to the door control struct
 * @returns Whether the door has detected any change
 */
int door_control_execute(DoorControl *door_control)
{
    if (door_control->relay_output == HIGH && timestamp_elapsed(door_control->relay_time, RELAY_DELAY))
    {
        gpio_put(RELAY_PIN, door_control->relay_output = LOW);
    }

    if (!timestamp_elapsed(door_control->time_stamp, DOOR_SAMPLE_RATE))
    {
        return 0;
    }
    else
    {
        door_control->time_stamp = time_us_32() + DOOR_SAMPLE_RATE;
    }

    switch (door_control->current_state)
    {
    case TRIGGER:
    {
        door_control->desired_state = DOOR_IDLE;
        door_control->result = SUCCESS;
    }
    default:
        break;
    }

    // State change
    if (door_control->desired_state != door_control->current_state)
    {
        if (door_control->desired_state == TRIGGER)
        {
            gpio_put(RELAY_PIN, door_control->relay_output = HIGH);
            door_control->command_time = time_us_32() + DOOR_COMMAND_DELAY;
            door_control->relay_time = time_us_32() + RELAY_DELAY;
        }
        door_control->current_state = door_control->desired_state;
        return 1;
    }

    return true;
}
