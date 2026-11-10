#include "Bed.h"
#include <vector>
#include "hardware/gpio.h"
#include "hardware/pwm.h"
#include <chrono>

#define LIGHT_PIN 5

#define GMT_OFFSET 39600000ULL

using namespace std;

Bed::Bed(Node *parent)
{
    gpio_set_function(LIGHT_PIN, GPIO_FUNC_PWM);

    auto lightOutSlice = pwm_gpio_to_slice_num(LIGHT_PIN);

    pwm_config config = pwm_get_default_config();
    // Set divider, reduces counter clock to sysclock/this value
    pwm_config_set_clkdiv(&config, 4.f);
    // Load the configuration into our PWM slice, and set it running.
    pwm_init(lightOutSlice, &config, true);

    pwm_set_gpio_level(LIGHT_PIN, 0);

    forceOn = BooleanMetric::create("forceOn", false);
    lightStatus = BooleanMetric::create("lightStatus", false);

    startHour = UInt8Metric::create("startHour", 19);
    startMinute = UInt8Metric::create("startMinute", 30);

    endHour = UInt8Metric::create("endHour", 22);
    endMinute = UInt8Metric::create("endMinute", 30);

    intensity = UInt8Metric::create("lightIntensity", 33);

    // intensity->addProperty();

    transitionTime = UInt16Metric::create("transitionTime", 750);

    std::vector<std::shared_ptr<UInt8Metric>> commands{
        startHour,
        startMinute,
        endHour,
        endMinute,
        intensity,
    };

    for (auto &command : commands)
    {
        command->setCommandCallback(
            [](__attribute__((unused)) Metric *metric, org_eclipse_tahu_protobuf_Payload_Metric *payload)
            {
                metric->setValue(&payload->value.int_value);
            });
    }

    transitionTime->setCommandCallback(
        [](__attribute__((unused)) Metric *metric, org_eclipse_tahu_protobuf_Payload_Metric *payload)
        {
            metric->setValue(&payload->value.int_value);
        });

    forceOn->setCommandCallback(
        [](__attribute__((unused)) Metric *metric, org_eclipse_tahu_protobuf_Payload_Metric *payload)
        {
            metric->setValue(&payload->value.boolean_value);
        });

    parent->addMetrics({
        forceOn,
        lightStatus,
        startHour,
        startMinute,
        endHour,
        endMinute,
        intensity,
        transitionTime,
    });
}

void Bed::sync(const time_t &time)
{
    // const auto &db = std::chrono::get_tzdb();

    auto millis = time;
    auto intermediate = ((time + GMT_OFFSET) / 60000ULL);
    auto minutes = intermediate % 60;
    auto hours = (intermediate / 60) % 24;

    // auto transition = (time_t)transitionTime->getValue();

    bool shouldLightBeOn = forceOn->getValue() ||
                           ((hours > startHour->getValue() || (hours == startHour->getValue() && minutes >= startMinute->getValue())) &&
                            (hours < endHour->getValue() || (hours == endHour->getValue() && minutes < endMinute->getValue())));

    if (shouldLightBeOn)
    {
        uint16_t expected = intensity->getValue();
        if (target != expected)
        {
            switch (state)
            {
            case LightState::ON:
            case LightState::OFF:
                state = LightState::TRANSITION;
                target = expected;
                transitionStart = millis;
                transitionEnd = millis + (time_t)transitionTime->getValue();
                break;
            case LightState::TRANSITION:
                target = expected;
                break;

            default:
                break;
            }
        }
    }
    else
    {
        uint16_t expected = 0;
        if (target != expected)
        {
            switch (state)
            {
            case LightState::OFF:
            case LightState::ON:
                state = LightState::TRANSITION;
                target = 0;
                transitionStart = millis;
                transitionEnd = millis + (time_t)transitionTime->getValue();
                break;
            case LightState::TRANSITION:
                target = 0;
                break;
            default:
                break;
            }
        }
    }

    lightStatus->setValue(shouldLightBeOn);

    if (target != current)
    {
        if (millis >= transitionEnd)
        {
            current = target;
        }
        else
        {
            float ratio = (float)(transitionEnd - millis) / (float)(transitionEnd - transitionStart);
            current = target == 0 ? ratio * current : (1.0 - ratio) * target;
        }

        double expectedPercent = ((double)current * 0.01);

        uint16_t expected = (expectedPercent * expectedPercent) * 0xFFFF;

        pwm_set_gpio_level(
            LIGHT_PIN,
            expected);
    }
    else
    {
        state = current == 0 ? LightState::OFF : LightState::ON;
    }
}