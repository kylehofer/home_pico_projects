/*
 * File: VictronParser.cpp
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

#include "VictronParser.h"
#include "properties/simple/StringProperty.h"
#include "properties/simple/UInt8Property.h"
#include "properties/complex/PropertySet.h"

#include "Fields.h"
#ifndef __AVR__
#include <cstdlib>
using namespace std;
#else
#include <Arduino.h>
#endif // __AVR__

// Character denoting the start of a new field
#define START_CHARACTER '\n'
// Character denoting the end of a field
#define END_CHARACTER '\r'
// Character denoting the split between a fields label and data
#define SPLIT_CHARACTER '\t'
// Character denoting an Async message
#define ASYNC_CHARACTER ':'

VictronParser::VictronParser() : fieldHandlerFunc(NULL), fieldHandlerClass(NULL), fieldCount(0), device(Device("Victron", 1000))
{
    configureSparkplug();
}

void VictronParser::configureSparkplug()
{
    device.addMetrics({
        batteryVoltage = Int32Metric::create("batteryVoltage", 0),
        panelVoltage = Int32Metric::create("panelVoltage", 0),
        current = Int16Metric::create("current", 0),
        yieldToday = Int16Metric::create("yieldToday", 0),
        yieldYesterday = Int16Metric::create("yieldYesterday", 0),
        maxPowerToday = Int16Metric::create("maxPowerToday", 0),
        maxPowerYesterday = Int16Metric::create("maxPowerYesterday", 0),
        daySequence = Int16Metric::create("daySequence", 0),
        operationState = UInt8Metric::create("operationState", 0),
        errorState = Int8Metric::create("errorState", 0),
        trackerOperationMode = Int8Metric::create("trackerOperationMode", 0),
        loadActive = BooleanMetric::create("loadActive", 0),
        productId = StringMetric::create("productId", ""),
        firmware = StringMetric::create("firmware", ""),
        serialNumber = StringMetric::create("serialNumber", ""),
    });

    batteryVoltage->addProperty(StringProperty::create("unit", "V"));
    panelVoltage->addProperty(StringProperty::create("unit", "V"));
    current->addProperty(StringProperty::create("unit", "A"));

    yieldToday->addProperty(StringProperty::create("unit", "W"));
    yieldYesterday->addProperty(StringProperty::create("unit", "W"));
    maxPowerToday->addProperty(StringProperty::create("unit", "W"));
    maxPowerYesterday->addProperty(StringProperty::create("unit", "W"));

    device.addMetric(operationState = UInt8Metric::create("operationState", 0));

    operationState->addProperty(
        PropertySet::create("enum",
                            {
                                UInt8Property::create("OFF", 0),
                                UInt8Property::create("FAULT", 2),
                                UInt8Property::create("BULK", 3),
                                UInt8Property::create("ABSORPTION", 4),
                                UInt8Property::create("FLOAT", 5),
                                UInt8Property::create("MANUAL_EQUALIZE", 7),
                                UInt8Property::create("STARTING", 245),
                                UInt8Property::create("AUTO_EQUALIZE", 247),
                                UInt8Property::create("EXTERNAL_CONTROL", 252),
                            }));

    errorState->addProperty(
        PropertySet::create("enum",
                            {
                                UInt8Property::create("NO_ERROR", 0),
                                UInt8Property::create("BATTERY_VOLTAGE_TOO_HIGH", 2),
                                UInt8Property::create("CHARGER_TEMPERATURE_TOO_HIGH", 17),
                                UInt8Property::create("CHARGER_OVER_CURRENT", 18),
                                UInt8Property::create("CHARGER_CURRENT_REVERSED", 19),
                                UInt8Property::create("BULK_TIME_EXCEEDED", 20),
                                UInt8Property::create("CURRENT_SENSOR_ISSUE", 21),
                                UInt8Property::create("TERMINALS_OVERHEATED", 26),
                                UInt8Property::create("SOLAR_VOLTAGE_TOO_HIGH", 33),
                                UInt8Property::create("SOLAR_CURRENT_TOO_HIGH", 34),
                                UInt8Property::create("INPUT_SHUTDOWN_BATTERY_VOLTAGE", 38),
                                UInt8Property::create("INPUT_SHUTDOWN_CURRENT", 39),
                                UInt8Property::create("USER_SETTING_INVALID", 119),
                            }));

    trackerOperationMode->addProperty(
        PropertySet::create("enum",
                            {
                                UInt8Property::create("OFF", 0),
                                UInt8Property::create("VOLTAGE_CURRENT_LIMITED", 1),
                                UInt8Property::create("MPP_TRACKER_ACTIVE", 2),
                            }));

    loadActive->addProperties(
        {
            StringProperty::create("trueText", "Active"),
            StringProperty::create("falseTest", "Deactive"),
        });
}

Device *VictronParser::getDevice()
{
    return &device;
}

VictronParser::VictronParser(VictronFieldHandler *fieldHandlerClass) : fieldHandlerFunc(NULL), fieldHandlerClass(fieldHandlerClass), fieldCount(0){};
VictronParser::VictronParser(void (*fieldHandlerFunc)(uint32_t, void *, size_t)) : fieldHandlerFunc(fieldHandlerFunc), fieldHandlerClass(NULL), fieldCount(0){};

VictronParser::~VictronParser()
{
    dumpFields();
}

void VictronParser::dumpFields()
{
    for (int i = (fieldCount - 1); i >= 0; i--)
    {
        delete fields[i];
    }

    fieldCount = 0;
}

void VictronParser::processFields()
{
    Field *field;
    for (int i = (fieldCount - 1); i >= 0; i--)
    {
        field = fields[i];
        switch (field->getId())
        {
        case VictronId::VOLTAGE:
            batteryVoltage->setValue(*((int32_t *)field->getData()));
            break;
        case VictronId::PANEL_VOLTAGE:
            panelVoltage->setValue(*((int32_t *)field->getData()));
            break;
        case VictronId::CURRENT:
            current->setValue(*((int32_t *)field->getData()));
            break;
        case VictronId::YIELD_TODAY:
            yieldToday->setValue(*((int16_t *)field->getData()));
            break;
        case VictronId::MAX_POWER_TODAY:
            maxPowerToday->setValue(*((int16_t *)field->getData()));
            break;
        case VictronId::YIELD_YESTERDAY:
            yieldYesterday->setValue(*((int16_t *)field->getData()));
            break;
        case VictronId::MAX_POWER_YESTERDAY:
            maxPowerYesterday->setValue(*((int16_t *)field->getData()));
            break;
        case VictronId::DAY_SEQUENCE:
            daySequence->setValue(*((int8_t *)field->getData()));
            break;
        case VictronId::OPERATION_STATE:
            operationState->setValue(*((uint8_t *)field->getData()));
            break;
        case VictronId::ERROR_STATE:
            errorState->setValue(*((uint8_t *)field->getData()));
            break;
        case VictronId::TRACKER_OPERATION_MODE:
            trackerOperationMode->setValue(*((uint8_t *)field->getData()));
            break;
        case VictronId::LOAD:
            loadActive->setValue(*((bool *)field->getData()));
            break;
        case VictronId::CHEC:
            break;
        case VictronId::PRODUCT_ID: // All Strings, Might implement in Modbus later
            productId->setValue((char *)field->getData());
            break;
        case VictronId::FIRMWARE:
            firmware->setValue((char *)field->getData());
        case VictronId::SERIAL_NUMBER:
            serialNumber->setValue((char *)field->getData());
        case VictronId::OFF_REASON:

        default:
            break;
        }

        delete field;
    }

    fieldCount = 0;
}

void VictronParser::parse(const char *buffer, int size)
{
    int index = 0;
    do
    {
        char input = buffer[index++];
        switch (state)
        {
        case VictronState::LABEL:
            // cout << "LABEL\n";
            if (input == SPLIT_CHARACTER)
            {
                state = VictronState::FIELD;
                fieldIndex = 0;
                memset(fieldData, 0, MAX_FIELD_LENGTH);
            }
            else if (input == ASYNC_CHARACTER)
            {
                // Data we don't care about, ignore it
                state = VictronState::ASYNC;
            }
            else if (labelIndex >= MAX_LABEL_LENGTH)
            {
                // Something went wrong and the label length is too long
                // Return to idle
                state = VictronState::IDLE;
            }
            else
            {
                labelData.buffer[labelIndex++] = input;
            }
            break;
        case VictronState::FIELD:
            if (input == END_CHARACTER)
            {
                state = VictronState::IDLE;
                // Process everything
                processEntry();
            }
            else if (fieldIndex >= MAX_FIELD_LENGTH)
            {
                // Something went wrong and the label length is too long
                // Return to idle
                state = VictronState::IDLE;
            }
            else
            {
                fieldData[fieldIndex++] = input;
            }
            break;
        case VictronState::IDLE:
        case VictronState::ASYNC:
            if (input == START_CHARACTER)
            {
                state = VictronState::LABEL;
                memset(labelData.buffer, 0, MAX_LABEL_LENGTH);
                labelIndex = 0;
            }
            break;
        default:
            break;
        }
        if (state != VictronState::ASYNC)
        {
            checksum += input;
        }
    } while (index < size);
}

void VictronParser::processEntry()
{
    // Victron specifies a max number of fields.
    // If we go over this count we might be overloading a queue.
    // We should dump our queue as we've got ourselves into a bad state.
    if (fieldCount >= MAX_FIELDS_COUNT)
    {
        dumpFields();
    }

    switch (labelData.lower)
    {
    case VictronId::VOLTAGE:
    case VictronId::PANEL_VOLTAGE:
        fields[fieldCount] = new Field(labelData.lower, atol(fieldData));
        fieldCount++;
        break;
    case VictronId::CURRENT:
    case VictronId::PANEL_POWER:
    case VictronId::LOAD_CURRENT:
    case VictronId::YIELD_TOTAL:
    case VictronId::YIELD_TODAY:
    case VictronId::MAX_POWER_TODAY:
    case VictronId::YIELD_YESTERDAY:
    case VictronId::MAX_POWER_YESTERDAY:
    case VictronId::DAY_SEQUENCE:
        fields[fieldCount] = new Field(labelData.lower, atoi(fieldData));
        fieldCount++;
        break;
    case VictronId::OPERATION_STATE:
    case VictronId::ERROR_STATE:
    case VictronId::TRACKER_OPERATION_MODE:
        fields[fieldCount] = new Field(labelData.lower, (int8_t)atoi(fieldData));
        fieldCount++;
        break;
    case VictronId::PRODUCT_ID:
    case VictronId::FIRMWARE:
    case VictronId::SERIAL_NUMBER:
        fields[fieldCount] = new Field(labelData.lower, fieldData, fieldIndex + 1);
        fieldCount++;
        break;
    case VictronId::LOAD: // ON/OFF value
        fields[fieldCount] = new Field(labelData.lower, (bool)(strcmp(fieldData, "ON") == 0));
        fieldCount++;
        break;
    case VictronId::CHEC:
        if (labelData.upper == VictronId::KSUM)
        {
            fields[fieldCount] = new Field(labelData.lower, checksum);
            fieldCount++;
            // if (checksum == 0)
            // {
            //     processFields();
            // }
            // else
            // {
            //     dumpFields();
            // }
            processFields();
            checksum = 0;
        }
        break;
    case VictronId::OFF_REASON:
    default:
        break;
    }

    if (fieldCount > 0)
    {
        // processFields();
    }
}
