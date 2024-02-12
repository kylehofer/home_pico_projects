#include <string.h>
#include <time.h>

#include <PicoSparkplugClient.h>

#include <Node.h>
#include <Device.h>
#include <metrics/simple/UInt8Metric.h>
#include <metrics/simple/Int32Metric.h>
#include <metrics/simple/DateTimeMetric.h>

#include <NtpClient.h>

#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "pico/multicore.h"
#include "pico/util/queue.h"
#include "hardware/gpio.h"
#include "hardware/adc.h"

#include "lwip/pbuf.h"
#include "lwip/tcp.h"

#define ADDRESS BROKER_ADDRESS
#define GROUP_ID "Garden"
#define NODE_ID "Bed"
#define HOST_ID "Home"

#define MAX_RETRIES 5000

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

void door_main()
{
}

void mqtt_test()
{
    char clientId[40];

    Uri uri(ADDRESS);
    sprintf(clientId, "garage_door_%ld", get_rand_32());

    NodeOptions nodeOptions = {
        GROUP_ID,
        NODE_ID,
        HOST_ID,
        100,
        NODE_CONTROL_REBIRTH};

    ClientOptions clientOptions = {
        .address = uri,
        .clientId = clientId,
        .username = NULL,
        .password = NULL,
        .connectTimeout = 5000,
        .keepAliveInterval = 5};

    Node node = Node(&nodeOptions);

    auto *client = node.addClient<PicoSparkplugClient>(&clientOptions);
    client->useNtpServer(NTP_ADDRESS, 123);

    node.enable();

    while (true)
    {
        wifi_connect();
        while (cyw43_wifi_link_status(&cyw43_state, CYW43_ITF_STA) == CYW43_LINK_JOIN)
        {
            printf("Wifi is connected, checking ntp server\n");
            uint32_t retries = 0;

            while (!node.isActive() && retries++ < MAX_RETRIES)
            {
#if PICO_CYW43_ARCH_POLL
                cyw43_arch_poll();
#endif
                sleep_ms(1);
            }

            if (retries >= MAX_RETRIES)
            {
                continue;
            }

            printf("Node is Active, starting Sparkplug execution\n");
            uint32_t executeTime = 5;
            while (node.isActive())
            {

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

    adc_init();

    multicore_launch_core1(door_main);

    if (cyw43_arch_init())
    {
        printf("failed to initialise\n");
        return 1;
    }
    cyw43_arch_enable_sta_mode();

    mqtt_test();

    cyw43_arch_deinit();

    return 0;
}