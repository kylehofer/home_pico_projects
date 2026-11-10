
#define DEBUGGING 1

#include "pico/stdlib.h"
#include <stdio.h>
#include <PicoSparkplugClient.h>
#include <DependencyExecutor.h>
#include <PicoOta.h>

#include "pico/cyw43_arch.h"
#include "pico/multicore.h"
#include "pico/util/queue.h"
#include "hardware/gpio.h"
#include "hardware/pwm.h"
#include "hardware/dma.h"
#include "hardware/watchdog.h"

#include "lwip/pbuf.h"
#include "lwip/tcp.h"

#include "VictronParser.h"
#include "Shed.h"

#include <PicoWifi.h>
#include <ElapsedTime.h>

#include <Node.h>
#include <Device.h>
#include <metrics/simple/UInt8Metric.h>
#include <metrics/simple/Int32Metric.h>
#include <metrics/simple/DateTimeMetric.h>
#include <metrics/simple/FixedStringMetric.h>

#define UART_ID uart0
#define BAUD_RATE 19200

#define DATA_BITS 8
#define STOP_BITS 1
#define PARITY UART_PARITY_NONE

#define UART_TX_PIN 0
#define UART_RX_PIN 1

#define ADDRESS BROKER_ADDRESS
//
#define GROUP_ID "Garden"
#define NODE_ID "Shed"
#define HOST_ID "Home"

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

void sparkplugExecute()
{
    char clientId[40];

    Uri uri(ADDRESS);
    sprintf(clientId, "garden_shed_%lu", get_rand_32());

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

    FixedStringMetric<15> address("ipAddress", "");
    FixedStringMetric<32> ssid("ssid", "");
    Int32Metric rssi("rssi", 0);

    node.addMetric(address);
    node.addMetric(ssid);
    node.addMetric(rssi);

    PicoOta ota(&node);

    node.addDevice(victronParser.getDevice());
    node.addClient<PicoSparkplugClient>(&clientOptions);

    NtpClient ntpClient(NTP_ADDRESS, 123, 3600);
    TimeManager::setInstance((TimeClient *)&ntpClient);

    node.enable();

    PicoWifi wifi;

    DependencyExecutor executor;
    ElapsedTime timeHandler;

    executor.addChain({
        std::bind(&PicoWifi::execute, &wifi),
        std::bind(&NtpClient::sync, &ntpClient),
        [&node, &timeHandler, &address, &wifi, &rssi, &ssid]()
        {
#if PICO_CYW43_ARCH_POLL
            cyw43_arch_poll();
#endif
            uint32_t elapsedTime = timeHandler.elapsed();
            if (elapsedTime > 0)
            {
                address.setValue(wifi.getAddress());
                ssid.setValue(wifi.getSsid());
                rssi.setValue(wifi.getRssi());

                node.execute(elapsedTime);
            }

            return true;
        },
        std::bind(&PicoOta::execute, &ota),
    });

    while (true)
    {
        if (!executor.execute())
        {
        }
        while (uart_is_readable(UART_ID))
        {
            char character = uart_getc(UART_ID);
            victronParser.parse(&character, 1);
        }
        shed.sync();
    }
}

int main()
{
    stdio_init_all();

    setupUart();

    sparkplugExecute();

    return 0;
}