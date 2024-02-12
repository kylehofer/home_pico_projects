/*
 * File: Fields.h
 * Project: gardener
 * Created Date: Friday October 21st 2022
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

#ifndef FIELDS
#define FIELDS

#include "enum.h"

#ifdef __AVR__
#include <stdint.h>
#include <Arduino.h>
#else
#include <cstdint>
#include <string>
#include <cstring>
using namespace std;
#endif // __AVR__

// Convert a set of characters into a unique binary ID
#define BUILD_RAW_ID(...) BUILD_RAW_ID_ARG(__VA_ARGS__, 0, 0, 0, 0)
#define BUILD_RAW_ID_ARG(_1, _2, _3, _4, ...) ((_1) + ((_2) << 8) + ((unsigned long)(_3) << 16) + ((unsigned long)(_4) << 24))

// Field Label Identifiers. We convert
BETTER_ENUM(VictronId, uint32_t,
            VOLTAGE = 'V',
            CURRENT = 'I',
            PANEL_VOLTAGE = BUILD_RAW_ID('V', 'P', 'V'),
            PANEL_POWER = BUILD_RAW_ID('P', 'P', 'V'),
            LOAD_CURRENT = BUILD_RAW_ID('I', 'L'),
            OPERATION_STATE = BUILD_RAW_ID('C', 'S'),
            ERROR_STATE = BUILD_RAW_ID('E', 'R', 'R'),
            LOAD = BUILD_RAW_ID('L', 'O', 'A', 'D'),
            YIELD_TOTAL = BUILD_RAW_ID('H', '1', '9'),
            YIELD_TODAY = BUILD_RAW_ID('H', '2', '0'),
            MAX_POWER_TODAY = BUILD_RAW_ID('H', '2', '1'),
            YIELD_YESTERDAY = BUILD_RAW_ID('H', '2', '2'),
            MAX_POWER_YESTERDAY = BUILD_RAW_ID('H', '2', '3'),
            PRODUCT_ID = BUILD_RAW_ID('P', 'I', 'D'),
            FIRMWARE = BUILD_RAW_ID('F', 'W'),
            SERIAL_NUMBER = BUILD_RAW_ID('S', 'E', 'R', '#'),
            TRACKER_OPERATION_MODE = BUILD_RAW_ID('M', 'P', 'P', 'T'),
            OFF_REASON = BUILD_RAW_ID('O', 'R'),
            DAY_SEQUENCE = BUILD_RAW_ID('H', 'S', 'D', 'S'),
            CHEC = BUILD_RAW_ID('C', 'h', 'e', 'c'),
            KSUM = BUILD_RAW_ID('k', 's', 'u', 'm'))

/**
 * @brief Base class for handling all Victron Fields.
 * Provides an Id/Data field combo.
 *
 */
class Field
{
private:
    uint32_t id;
    size_t size;
    void *data;

public:
    /**
     * @brief Construct a new Field object
     *
     * @param id Unique identifier for the field
     */
    template <typename T>
    Field(uint32_t id, T data) : id(id), size(sizeof(T))
    {
        this->data = malloc(sizeof(T));
        *((T *)this->data) = data;
    };

    /**
     * @brief Construct a new Field object
     *
     * @param id Unique identifier for the field
     */
    template <typename T>
    Field(uint32_t id, T *data, size_t size) : id(id)
    {
        this->data = malloc(size);
        this->size = size;
        memcpy(this->data, data, size);
    };

    ~Field()
    {
        free(data);
    };

    /**
     * @brief Get the Id object
     *
     * @return int
     */
    uint32_t getId() { return id; };
    void *getData() { return data; };
    size_t getSize() { return size; };
};

#endif /* FIELDS */
