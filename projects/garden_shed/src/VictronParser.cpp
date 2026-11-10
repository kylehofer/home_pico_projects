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

#include "Fields.h"
#ifndef __AVR__
#include <cstdlib>
using namespace std;
#else
#include <Arduino.h>
#endif // __AVR__

// Character denoting the start of a Field
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

    device.addMetric(batteryVoltage);
    device.addMetric(panelVoltage);
    device.addMetric(current);
    device.addMetric(power);
    device.addMetric(yieldToday);
    device.addMetric(yieldYesterday);
    device.addMetric(maxPowerToday);
    device.addMetric(maxPowerYesterday);
    device.addMetric(daySequence);
    device.addMetric(operationState);
    device.addMetric(errorState);
    device.addMetric(trackerOperationMode);
    device.addMetric(loadActive);
    device.addMetric(productId);
    device.addMetric(firmware);
    device.addMetric(serialNumber);

    batteryVoltage.addProperty(millivolts);
    panelVoltage.addProperty(millivolts);
    current.addProperty(milliamps);
    power.addProperty(watts);

    yieldToday.addProperty(watts);
    yieldYesterday.addProperty(watts);
    maxPowerToday.addProperty(watts);
    maxPowerYesterday.addProperty(watts);

    operationStateEnum.addProperty(off);
    operationStateEnum.addProperty(fault);
    operationStateEnum.addProperty(bulk);
    operationStateEnum.addProperty(absorption);
    operationStateEnum.addProperty(floatProperty);
    operationStateEnum.addProperty(manualEqualize);
    operationStateEnum.addProperty(starting);
    operationStateEnum.addProperty(autoEqualize);
    operationStateEnum.addProperty(externalControl);

    operationState.addProperty(operationStateEnum);

    errorStateEnum.addProperty(noError);
    errorStateEnum.addProperty(batteryVoltageTooHigh);
    errorStateEnum.addProperty(chargerTemperatureTooHigh);
    errorStateEnum.addProperty(chargerOverCurrent);
    errorStateEnum.addProperty(chargerCurrentReversed);
    errorStateEnum.addProperty(bulkTimeExceeded);
    errorStateEnum.addProperty(currentSensorIssue);
    errorStateEnum.addProperty(terminalsOverheated);
    errorStateEnum.addProperty(solarVoltageTooHigh);
    errorStateEnum.addProperty(solarCurrentTooHigh);
    errorStateEnum.addProperty(inputShutdownBatteryVoltage);
    errorStateEnum.addProperty(inputShutdownCurrent);
    errorStateEnum.addProperty(userSettingInvalid);

    errorState.addProperty(errorStateEnum);

    trackerOperationModeEnum.addProperty(off);
    trackerOperationModeEnum.addProperty(voltageCurrentLimited);
    trackerOperationModeEnum.addProperty(mppTrackerActive);

    trackerOperationMode.addProperty(trackerOperationModeEnum);
}

Device *VictronParser::getDevice()
{
    return &device;
}

VictronParser::VictronParser(VictronFieldHandler *fieldHandlerClass) : fieldHandlerFunc(NULL), fieldHandlerClass(fieldHandlerClass), fieldCount(0) {};
VictronParser::VictronParser(void (*fieldHandlerFunc)(uint32_t, void *, size_t)) : fieldHandlerFunc(fieldHandlerFunc), fieldHandlerClass(NULL), fieldCount(0) {};

VictronParser::~VictronParser()
{
    dumpFields();
}

void VictronParser::dumpFields()
{
    for (int i = (fieldCount - 1); i >= 0; i--)
    {
        fields[i].clear();
    }

    fieldCount = 0;
}

void VictronParser::processFields()
{
    for (int i = (fieldCount - 1); i >= 0; i--)
    {
        Field *field = &fields[i];
        switch (field->getId())
        {
        case VictronId::VOLTAGE:
            batteryVoltage.setValue(*((int32_t *)field->getData()));
            power.setValue((double)((int32_t)current.getValue() * batteryVoltage.getValue()) / 1000000);
            break;
        case VictronId::PANEL_VOLTAGE:
            panelVoltage.setValue(*((int32_t *)field->getData()));
            break;
        case VictronId::CURRENT:
            current.setValue(*((int16_t *)field->getData()));
            power.setValue((double)((int32_t)current.getValue() * batteryVoltage.getValue()) / 1000000.0);
            break;
        case VictronId::YIELD_TODAY:
            yieldToday.setValue(*((int16_t *)field->getData()));
            break;
        case VictronId::MAX_POWER_TODAY:
            maxPowerToday.setValue(*((int16_t *)field->getData()));
            break;
        case VictronId::YIELD_YESTERDAY:
            yieldYesterday.setValue(*((int16_t *)field->getData()));
            break;
        case VictronId::MAX_POWER_YESTERDAY:
            maxPowerYesterday.setValue(*((int16_t *)field->getData()));
            break;
        case VictronId::DAY_SEQUENCE:
            daySequence.setValue(*((int8_t *)field->getData()));
            break;
        case VictronId::OPERATION_STATE:
            operationState.setValue(*((uint8_t *)field->getData()));
            break;
        case VictronId::ERROR_STATE:
            errorState.setValue(*((uint8_t *)field->getData()));
            break;
        case VictronId::TRACKER_OPERATION_MODE:
            trackerOperationMode.setValue(*((uint8_t *)field->getData()));
            break;
        case VictronId::LOAD:
            loadActive.setValue(*((bool *)field->getData()));
            break;
        case VictronId::PRODUCT_ID: // All Strings, Might implement in Modbus later
            productId.setValue((char *)field->getData());
            break;
        case VictronId::FIRMWARE:
            firmware.setValue((char *)field->getData());
            break;
        case VictronId::SERIAL_NUMBER:
            serialNumber.setValue((char *)field->getData());
            break;
        case VictronId::OFF_REASON:
        case VictronId::CHEC:
        case VictronId::KSUM:
        default:
            break;
        }

        field->clear();
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
        fields[fieldCount++] = Field(labelData.lower, atol(fieldData));
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
        fields[fieldCount++] = Field(labelData.lower, atoi(fieldData));
        break;
    case VictronId::OPERATION_STATE:
    case VictronId::ERROR_STATE:
    case VictronId::TRACKER_OPERATION_MODE:
        fields[fieldCount++] = Field(labelData.lower, (int8_t)atoi(fieldData));
        break;
    case VictronId::PRODUCT_ID:
    case VictronId::FIRMWARE:
    case VictronId::SERIAL_NUMBER:
        fields[fieldCount++] = Field(labelData.lower, fieldData, fieldIndex + 1);

        break;
    case VictronId::LOAD: // ON/OFF value
        fields[fieldCount++] = Field(labelData.lower, (bool)(strcmp(fieldData, "ON") == 0));
        break;
    case VictronId::CHEC:
        if (labelData.upper == VictronId::KSUM)
        {
            fields[fieldCount++] = Field(labelData.lower, checksum);
            if (checksum == 0)
            {
                processFields();
            }
            else
            {
                dumpFields();
            }
            checksum = 0;
        }
        break;
    case VictronId::OFF_REASON:
        break;
    default:
        printf("Could not determine field: %lu\n", labelData.lower);
        printf("Characters: ");

        int i = 0;

        while (labelData.buffer[i] > 0 && i < MAX_LABEL_LENGTH)
        {
            printf("%c", labelData.buffer[i++]);
        }
        printf("\n");
        break;
    }
}
