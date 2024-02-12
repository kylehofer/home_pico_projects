#include "Shed.h"
#include "hardware/gpio.h"
#include "hardware/pwm.h"

#define LIGHT_PIN 9
#define DOOR_SENSOR_PIN 3

#define CLOCK 125000000

Shed::Shed(Publishable *parent)
{
    gpio_init(DOOR_SENSOR_PIN);
    gpio_set_dir(DOOR_SENSOR_PIN, GPIO_IN);
    gpio_pull_down(DOOR_SENSOR_PIN);
    gpio_set_function(LIGHT_PIN, GPIO_FUNC_PWM);

    lightOutSlice = pwm_gpio_to_slice_num(LIGHT_PIN);
    // lightPwmChannel = pwm_gpio_to_channel(LIGHT_PIN);
    // lightPwmWrap = configurePwmDuty(lightOutSlice, lightPwmChannel, LIGHT_FREQUENCY);

    // Get some sensible defaults for the slice configuration. By default, the
    // counter is allowed to wrap over its maximum range (0 to 2**16-1)
    pwm_config config = pwm_get_default_config();
    // Set divider, reduces counter clock to sysclock/this value
    pwm_config_set_clkdiv(&config, 4.f);
    // Load the configuration into our PWM slice, and set it running.
    pwm_init(lightOutSlice, &config, true);

    pwm_set_gpio_level(LIGHT_PIN, 0);

    doorOpen = BooleanMetric::create("doorOpen", false);
    intensity = UInt8Metric::create("lightIntensity", 33);

    intensity->setCommandCallback(
        [](__attribute__((unused)) Metric *metric, org_eclipse_tahu_protobuf_Payload_Metric *payload)
        {
            metric->setValue(&payload->value.boolean_value);
        });

    parent->addMetrics({doorOpen, intensity});
}

void Shed::sync()
{
    doorOpen->setValue(!gpio_get(DOOR_SENSOR_PIN));

    pwm_set_gpio_level(
        LIGHT_PIN,
        doorOpen->getValue() ? intensity->getValue() * 255 : 0);
}
