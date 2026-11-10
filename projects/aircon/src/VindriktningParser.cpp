/*
 * File: VindriktningParser.cpp
 * Project: gardener
 * Created Date: Monday November 7th 2022
 * Author: Kyle Hofer
 *
 * MIT License
 *
 * Copyright (c) 2022 Kyle Hofer
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is furnished to do
 * so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * HISTORY:
 */

#include "VindriktningParser.h"

#ifndef __AVR__
#include <cstdint>
#else
#include <Arduino.h>
#endif // __AVR__

#include <Device.h>
#include <metrics/simple/UInt16Metric.h>

#include <pico/stdlib.h>
#include <pico/multicore.h>
#include <atomic>

#ifndef __AVR__
#include <cstdlib>
using namespace std;
#else
#include <Arduino.h>
#endif // __AVR__

// State of the Vindriktning serial
enum class VindriktningState
{
    IDLE,
    HEADER_1,
    HEADER_2,
    HEADER_3,
    CHECKSUM
};

#define HEADER_SIZE 3
#define CHECKSUM_SIZE 20

#define UART_ID uart0
#define BAUD_RATE 9600

#define DATA_BITS 8
#define STOP_BITS 1
#define PARITY UART_PARITY_NONE

#define UART_TX_PIN 0
#define UART_RX_PIN 1

#define START_CHARACTER 0x16
#define HEADER_1_CHARACTER 0x16
#define HEADER_2_CHARACTER 0x11
#define HEADER_3_CHARACTER 0x0B

int dataCount;
VindriktningState state = VindriktningState::IDLE;
int8_t checksum;
char packetData[CHECKSUM_SIZE];

std::atomic<bool> hasData;

int16_t pm25Value = 0;
std::shared_ptr<UInt16Metric> pm25Metric;

void parserConfig(Publishable *publishable)
{
    if (!pm25Metric)
    {
        pm25Metric = UInt16Metric::create("pm25", 0);
    }

    publishable->addMetric(pm25Metric);
}

void inline parse(const char input)
{
    switch (state)
    {
    case VindriktningState::HEADER_1:
        if (input == HEADER_2_CHARACTER)
        {
            state = VindriktningState::HEADER_2;
        }
        else
        {
            dataCount = 0;
            state = VindriktningState::IDLE;
        }
        break;
    case VindriktningState::HEADER_2:
        if (input == HEADER_3_CHARACTER)
        {
            state = VindriktningState::CHECKSUM;
        }
        else
        {
            dataCount = 0;
            state = VindriktningState::IDLE;
        }
        break;
    case VindriktningState::CHECKSUM:
        break;
    case VindriktningState::IDLE:
        if (input == START_CHARACTER)
        {
            dataCount = 0;
            checksum = 0;
            state = VindriktningState::HEADER_1;
        }
        break;
    default:
        break;
    }
    if (state != VindriktningState::IDLE)
    {
        packetData[dataCount++] = input;
        checksum += input;
    }

    if (dataCount == (CHECKSUM_SIZE))
    {
        state = VindriktningState::IDLE;
        dataCount = 0;
        checksum = 0;
        pm25Value = (packetData[5] << 8) | packetData[6];
        hasData = true;
    }
}

void onUartReceive()
{
    while (uart_is_readable(UART_ID))
    {
        parse(uart_getc(UART_ID));
    }
}

void setupUart()
{
    // Set up our UART with the required speed.
    uart_init(UART_ID, BAUD_RATE);

    // Set the TX and RX pins by using the function select on the GPIO
    // Set datasheet for more information on function select
    gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);

    uart_set_baudrate(UART_ID, BAUD_RATE);

    // Set UART flow control CTS/RTS, we don't want these, so turn them off
    uart_set_hw_flow(UART_ID, false, false);

    // Set our packetData format
    uart_set_format(UART_ID, DATA_BITS, STOP_BITS, PARITY);

    // Turn off FIFO's - we want to do this character by character
    uart_set_fifo_enabled(UART_ID, false);

    // Set up a RX interrupt
    // We need to set up the handler first
    // Select correct interrupt for the UART we are using
    int UART_IRQ = UART_ID == uart0 ? UART0_IRQ : UART1_IRQ;

    // And set up and enable the interrupt handlers
    irq_set_exclusive_handler(UART_IRQ, onUartReceive);
    irq_set_enabled(UART_IRQ, true);

    // Now enable the UART to send interrupts - RX only
    uart_set_irq_enables(UART_ID, true, false);
}

void uartMain()
{
    setupUart();
    while (1)
        tight_loop_contents();
    // {
    //     if (hasData && mutex_try_enter(&uartMutex, nullptr))
    // {

    // }
    // }
}

void startParser()
{
    multicore_launch_core1(uartMain);
}

void syncParser()
{
    if (hasData)
    {
        pm25Metric->setValue(pm25Value);
        hasData = false;
    }
}
