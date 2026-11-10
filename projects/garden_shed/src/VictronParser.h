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

#ifndef PROJECTS_GARDEN_SHED_SRC_VICTRONPARSER
#define PROJECTS_GARDEN_SHED_SRC_VICTRONPARSER

#include "Fields.h"
#ifndef __AVR__
#include <cstdint>
#else
#include <Arduino.h>
#endif // __AVR__

#include "Device.h"
#include "metrics/simple/DoubleMetric.h"
#include "metrics/simple/Int32Metric.h"
#include "metrics/simple/Int16Metric.h"
#include "metrics/simple/Int8Metric.h"
#include "metrics/simple/UInt8Metric.h"
#include "metrics/simple/StringMetric.h"
#include "metrics/simple/BooleanMetric.h"
#include "metrics/simple/FixedStringMetric.h"
#include "properties/simple/FixedStringProperty.h"
#include "properties/simple/UInt8Property.h"
#include "properties/complex/PropertySet.h"

using namespace Sparkplug;

// Max length of a Victron Field Label
#define MAX_LABEL_LENGTH 8
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
    VictronState state = VictronState::IDLE;
    bool initialized;
    int8_t checksum;
    LabelToBuffer_u labelData;
    int labelIndex;
    char fieldData[MAX_FIELD_LENGTH];
    int fieldIndex;
    Field fields[MAX_FIELDS_COUNT];
    void (*fieldHandlerFunc)(uint32_t, void *, size_t);
    VictronFieldHandler *fieldHandlerClass;
    int fieldCount;

    Device device;
    Int32Metric batteryVoltage = Int32Metric("batteryVoltage", 0);
    Int32Metric panelVoltage = Int32Metric("panelVoltage", 0);
    Int16Metric current = Int16Metric("current", 0);
    DoubleMetric power = DoubleMetric("power", 0.0);
    Int16Metric yieldToday = Int16Metric("yieldToday", 0);
    Int16Metric yieldYesterday = Int16Metric("yieldYesterday", 0);
    Int16Metric maxPowerToday = Int16Metric("maxPowerToday", 0);
    Int16Metric maxPowerYesterday = Int16Metric("maxPowerYesterday", 0);
    Int16Metric daySequence = Int16Metric("daySequence", 0);
    UInt8Metric operationState = UInt8Metric("operationState", 0);
    Int8Metric errorState = Int8Metric("errorState", 0);
    Int8Metric trackerOperationMode = Int8Metric("trackerOperationMode", 0);
    BooleanMetric loadActive = BooleanMetric("loadActive", 0);
    FixedStringMetric<32> productId = FixedStringMetric<32>("productId", "");
    FixedStringMetric<32> firmware = FixedStringMetric<32>("firmware", "");
    FixedStringMetric<32> serialNumber = FixedStringMetric<32>("serialNumber", "");

    /** Properties */

    FixedStringProperty<3> millivolts = FixedStringProperty<3>("unit", "mV");
    FixedStringProperty<3> milliamps = FixedStringProperty<3>("unit", "mA");
    FixedStringProperty<2> watts = FixedStringProperty<2>("unit", "W");

    FixedStringProperty<7> activate = FixedStringProperty<7>("trueText", "Active");
    FixedStringProperty<11> deactivate = FixedStringProperty<11>("falseText", "Deactivate");

    PropertySet operationStateEnum = PropertySet("enum");
    PropertySet errorStateEnum = PropertySet("enum");
    PropertySet trackerOperationModeEnum = PropertySet("enum");

    /** enums */
    UInt8Property noError = UInt8Property("NO_ERROR", 0);
    UInt8Property batteryVoltageTooHigh = UInt8Property("BATTERY_VOLTAGE_TOO_HIGH", 2);
    UInt8Property chargerTemperatureTooHigh = UInt8Property("CHARGER_TEMPERATURE_TOO_HIGH", 17);
    UInt8Property chargerOverCurrent = UInt8Property("CHARGER_OVER_CURRENT", 18);
    UInt8Property chargerCurrentReversed = UInt8Property("CHARGER_CURRENT_REVERSED", 19);
    UInt8Property bulkTimeExceeded = UInt8Property("BULK_TIME_EXCEEDED", 20);
    UInt8Property currentSensorIssue = UInt8Property("CURRENT_SENSOR_ISSUE", 21);
    UInt8Property terminalsOverheated = UInt8Property("TERMINALS_OVERHEATED", 26);
    UInt8Property solarVoltageTooHigh = UInt8Property("SOLAR_VOLTAGE_TOO_HIGH", 33);
    UInt8Property solarCurrentTooHigh = UInt8Property("SOLAR_CURRENT_TOO_HIGH", 34);
    UInt8Property inputShutdownBatteryVoltage = UInt8Property("INPUT_SHUTDOWN_BATTERY_VOLTAGE", 38);
    UInt8Property inputShutdownCurrent = UInt8Property("INPUT_SHUTDOWN_CURRENT", 39);
    UInt8Property userSettingInvalid = UInt8Property("USER_SETTING_INVALID", 119);

    UInt8Property off = UInt8Property("OFF", 0);
    UInt8Property fault = UInt8Property("FAULT", 2);
    UInt8Property bulk = UInt8Property("BULK", 3);
    UInt8Property absorption = UInt8Property("ABSORPTION", 4);
    UInt8Property floatProperty = UInt8Property("FLOAT", 5);
    UInt8Property manualEqualize = UInt8Property("MANUAL_EQUALIZE", 7);
    UInt8Property starting = UInt8Property("STARTING", 245);
    UInt8Property autoEqualize = UInt8Property("AUTO_EQUALIZE", 247);
    UInt8Property externalControl = UInt8Property("EXTERNAL_CONTROL", 252);

    UInt8Property voltageCurrentLimited = UInt8Property("VOLTAGE_CURRENT_LIMITED", 1);
    UInt8Property mppTrackerActive = UInt8Property("MPP_TRACKER_ACTIVE", 2);

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

#endif /* PROJECTS_GARDEN_SHED_SRC_VICTRONPARSER */
