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

#include "door_control.h"

#define TCP_PORT 4242
#define DEBUG_printf printf
#define BUF_SIZE 2048

#define ADDRESS BROKER_ADDRESS
#define GROUP_ID "Garage"
#define NODE_ID "Door"
#define HOST_ID "Home"

#define TEST_ITERATIONS 10
#define POLL_TIME_S 5

#define MAX_RETRIES 5000

static queue_t sparkplugQueue;
static queue_t doorQueue;

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

    DoorControl doorControl;
    DoorControl *doorControlPtr = &doorControl;
    door_control_initialize(doorControlPtr, 220, 4095);

    while (true)
    {
        if (!queue_is_empty(&sparkplugQueue))
        {
            uint8_t value;
            queue_remove_blocking(&sparkplugQueue, &value);
            door_control_set_position(doorControlPtr, value);
        }

        if (door_control_execute(doorControlPtr) && queue_is_empty(&doorQueue))
        {

            DoorData doorData = door_control_get(doorControlPtr);
            queue_add_blocking(&doorQueue, &doorData);
        }
    }
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

    auto position = UInt8Metric::create("position", 0);

    position->setCommandCallback(
        [](__attribute__((unused)) Metric *metric, org_eclipse_tahu_protobuf_Payload_Metric *payload)
        {
            if (queue_is_empty(&sparkplugQueue))
            {
                queue_add_blocking(&sparkplugQueue, &payload->value);
            }
        });

    auto state = UInt8Metric::create("state", 0);
    auto result = UInt8Metric::create("result", 0);

    node.addMetric(position);
    node.addMetric(state);
    node.addMetric(result);

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

            uint32_t executeTime = 5;
            while (node.isActive())
            {
                if (!queue_is_empty(&doorQueue))
                {
                    DoorData value;
                    queue_remove_blocking(&doorQueue, &value);
                    position->setValue(value.position);
                    state->setValue(value.state);
                    result->setValue(value.result);
                }

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

    queue_init(&sparkplugQueue, sizeof(uint8_t), 1);
    queue_init(&doorQueue, sizeof(DoorData), 1);

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