/*
 * File: VictronParser.h
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

#ifndef VICTRONPARSER
#define VICTRONPARSER

#include "Fields.h"
#ifndef __AVR__
#include <cstdint>
#else
#include <Arduino.h>
#endif // __AVR__

#include "Device.h"
#include "metrics/simple/Int32Metric.h"
#include "metrics/simple/Int16Metric.h"
#include "metrics/simple/Int8Metric.h"
#include "metrics/simple/UInt8Metric.h"
#include "metrics/simple/StringMetric.h"
#include "metrics/simple/BooleanMetric.h"

// Max length of a Victron Field Label
#define MAX_LABEL_LENGTH 8
// Max length of a Victron Field Value
#define MAX_FIELD_LENGTH 32
// Maximum number of fields that in a single block with a checksum
#define MAX_FIELDS_COUNT 22

// State of the Victron serial
enum class VictronState
{
    IDLE,
    LABEL,
    FIELD,
    ASYNC,
    CHECKSUM
};

// Used to convert Field labels to binary values for fast comparisons
typedef union
{
    unsigned char buffer[MAX_LABEL_LENGTH];
    struct
    {
        uint32_t lower;
        uint32_t upper;
    };
} LabelToBuffer_u;

class VictronFieldHandler
{
private:
protected:
public:
    virtual void fieldUpdate(VictronId id, void *data, size_t size) = 0;
};

class VictronParser
{
private:
    VictronState state;
    bool initialized;
    int8_t checksum;
    LabelToBuffer_u labelData;
    int labelIndex;
    char fieldData[MAX_FIELD_LENGTH];
    int fieldIndex;
    Field *fields[MAX_FIELDS_COUNT];
    void (*fieldHandlerFunc)(uint32_t, void *, size_t);
    VictronFieldHandler *fieldHandlerClass;
    int fieldCount;

    Device device;
    std::shared_ptr<Int32Metric> batteryVoltage;
    std::shared_ptr<Int32Metric> panelVoltage;
    std::shared_ptr<Int16Metric> current;
    std::shared_ptr<Int16Metric> yieldToday;
    std::shared_ptr<Int16Metric> yieldYesterday;
    std::shared_ptr<Int16Metric> maxPowerToday;
    std::shared_ptr<Int16Metric> maxPowerYesterday;
    std::shared_ptr<Int16Metric> daySequence;
    std::shared_ptr<UInt8Metric> operationState;
    std::shared_ptr<Int8Metric> errorState;
    std::shared_ptr<Int8Metric> trackerOperationMode;
    std::shared_ptr<BooleanMetric> loadActive;
    std::shared_ptr<StringMetric> productId;
    std::shared_ptr<StringMetric> firmware;
    std::shared_ptr<StringMetric> serialNumber;

    void configureSparkplug();

    /**
     * @brief Process an single field line containing a Label/Data combo
     *
     */
    void processEntry();

    /**
     * @brief Processes all fields in the queue
     *
     */
    void processFields();

    /**
     * @brief Frees all memory held by the fields queue
     *
     */
    void dumpFields();

protected:
public:
    VictronParser();
    VictronParser(VictronFieldHandler *dataHandler);
    VictronParser(void (*function)(uint32_t, void *, size_t));
    ~VictronParser();

    /**
     * @brief Process a buffer read from the victron serial
     *
     * @param buffer buffer read from the serial
     * @param size The number of bytes read
     */
    void parse(const char *buffer, int size);
    Device *getDevice();
};

#endif /* VICTRONPARSER */
