#include "pico/stdlib.h"
#include <stdio.h>

#include <PicoSparkplugClient.h>

#include "pico/cyw43_arch.h"
#include "pico/multicore.h"
#include "pico/util/queue.h"
#include "hardware/gpio.h"
#include "hardware/pwm.h"

#include "lwip/pbuf.h"
#include "lwip/tcp.h"

#include <Node.h>
#include <Device.h>
#include <metrics/simple/UInt8Metric.h>
#include <metrics/simple/BooleanMetric.h>

#define FAN_PIN 10

#include <chrono>

#define UART_ID uart0
#define BAUD_RATE 19200

#define DATA_BITS 8
#define STOP_BITS 1
#define PARITY UART_PARITY_NONE

#define UART_TX_PIN 0
#define UART_RX_PIN 1

#define ADDRESS BROKER_ADDRESS
#define GROUP_ID "Home"
#define NODE_ID "Couch"
#define HOST_ID "Home"

#define GMT_OFFSET 39600000ULL

#define MAX_RETRIES 5000

inline bool nodeConnect(Node &node)
{
    uint32_t retries = 0;

    while (!node.isActive() && retries++ < MAX_RETRIES)
    {
#if PICO_CYW43_ARCH_POLL
        cyw43_arch_poll();
#endif
        sleep_ms(1);
    }

    return node.isActive();
}

inline bool ntpConnect(NtpClient &client)
{
    uint32_t retries = 0;
    while (!client.synced() && retries++ < MAX_RETRIES)
    {
        client.sync();
#if PICO_CYW43_ARCH_POLL
        cyw43_arch_poll();
#endif
        sleep_ms(1);
    }

    return client.synced();
}

void wifi_connect()
{
    printf("Connecting to WiFi...\n");
    if (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 30000))
    {
        printf("failed to connect.\n");
        return;
    }
    else
    {
        printf("WIFI Connected.\n");
    }
}

void sparkplugExecute()
{
    gpio_set_function(FAN_PIN, GPIO_FUNC_PWM);

    auto lightOutSlice = pwm_gpio_to_slice_num(FAN_PIN);

    pwm_config config = pwm_get_default_config();
    // Set divider, reduces counter clock to sysclock/this value
    pwm_config_set_clkdiv(&config, 4.f);
    // Load the configuration into our PWM slice, and set it running.
    pwm_init(lightOutSlice, &config, true);

    pwm_set_gpio_level(FAN_PIN, 0);

    char clientId[40];

    Uri uri(ADDRESS);
    sprintf(clientId, "garden_shed_%ld", get_rand_32());

    NodeOptions nodeOptions = {
        GROUP_ID,
        NODE_ID,
        NULL,
        1000,
        NODE_CONTROL_REBIRTH};

    ClientOptions clientOptions = {
        .address = uri,
        .clientId = clientId,
        .username = NULL,
        .password = NULL,
        .connectTimeout = 5000,
        .keepAliveInterval = 5};

    Node node = Node(&nodeOptions);

    node.addClient<PicoSparkplugClient>(&clientOptions);

    NtpClient ntpClient(NTP_ADDRESS, 123, 3600);
    TimeManager::setInstance((TimeClient *)&ntpClient);

    auto fanSpeed = UInt8Metric::create("fanSpeed", 75);
    auto fanEnable = BooleanMetric::create("fanEnable", false);

    fanSpeed->setCommandCallback(
        [](__attribute__((unused)) Metric *metric, org_eclipse_tahu_protobuf_Payload_Metric *payload)
        {
            metric->setValue(&payload->value.int_value);
        });

    fanEnable->setCommandCallback(
        [](__attribute__((unused)) Metric *metric, org_eclipse_tahu_protobuf_Payload_Metric *payload)
        {
            metric->setValue(&payload->value.boolean_value);
        });
    node.addMetrics({fanEnable, fanSpeed});

    node.enable();

    while (true)
    {
        wifi_connect();
        while (cyw43_wifi_link_status(&cyw43_state, CYW43_ITF_STA) == CYW43_LINK_JOIN)
        {
            printf("Wifi is connected\n");

            if (!ntpConnect(ntpClient))
            {
                printf("NTP Client failed to sync\n");
                continue;
            }

            if (!nodeConnect(node))
            {
                printf("Node failed to activate\n");
                continue;
            }

            printf("Node is Active, starting Sparkplug execution\n");
            uint32_t executeTime = 5;
            while (node.isActive())
            {
                pwm_set_gpio_level(
                    FAN_PIN,
                    fanEnable->getValue() ? fanSpeed->getValue() * 6553 : 0);

                node.execute(executeTime);
                sleep_ms(executeTime);

#if PICO_CYW43_ARCH_POLL
                cyw43_arch_poll();
#endif
            }
            printf("Node is no longer Active, checking wifi status\n");
        }
        printf("WIFI not connected, waiting 5 seconds\n");
        sleep_ms(5000);
    }
}

int main()
{
    stdio_init_all();

    if (cyw43_arch_init())
    {
        printf("failed to initialise\n");
        return 1;
    }

    cyw43_arch_enable_sta_mode();

    sparkplugExecute();

    cyw43_arch_deinit();

    return 0;
}
