
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

#include "VictronParser.h"
#include "Shed.h"

#include <Node.h>
#include <Device.h>
#include <metrics/simple/UInt8Metric.h>
#include <metrics/simple/Int32Metric.h>
#include <metrics/simple/DateTimeMetric.h>

#define UART_ID uart0
#define BAUD_RATE 19200

#define DATA_BITS 8
#define STOP_BITS 1
#define PARITY UART_PARITY_NONE

#define UART_TX_PIN 0
#define UART_RX_PIN 1

#define ADDRESS BROKER_ADDRESS
#define GROUP_ID "Garden"
#define NODE_ID "Shed"
#define HOST_ID "Home"

#define MAX_RETRIES 5000

void setupUart()
{
    // Set up our UART with the required speed.
    uart_init(UART_ID, BAUD_RATE);

    // Set the TX and RX pins by using the function select on the GPIO
    // Set datasheet for more information on function select
    gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);

    uart_set_baudrate(UART_ID, BAUD_RATE);

    // Set UART flow control CTS/RTS, we don't want these, so turn them off
    uart_set_hw_flow(UART_ID, false, false);

    // Set our data format
    uart_set_format(UART_ID, DATA_BITS, STOP_BITS, PARITY);

    // Turn off FIFO's - we want to do this character by character
    uart_set_fifo_enabled(UART_ID, true);
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
    char clientId[40];

    Uri uri(ADDRESS);
    sprintf(clientId, "garden_shed_%ld", get_rand_32());

    NodeOptions nodeOptions = {
        GROUP_ID,
        NODE_ID,
        HOST_ID,
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

    VictronParser victronParser;

    Shed shed((Publishable *)&node);

    node.addDevice(victronParser.getDevice());
    auto client = node.addClient<PicoSparkplugClient>(&clientOptions);
    client->useNtpServer(NTP_ADDRESS, 123);

    node.enable();

    while (true)
    {
        wifi_connect();
        while (cyw43_wifi_link_status(&cyw43_state, CYW43_ITF_STA) == CYW43_LINK_JOIN)
        {
            printf("Wifi is connected\n");
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
                while (uart_is_readable(UART_ID))
                {
                    char character = uart_getc(UART_ID);
                    victronParser.parse(&character, 1);
                }

                shed.sync();

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

    setupUart();

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