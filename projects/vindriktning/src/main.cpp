
#include <stdio.h>
#include <pico/stdlib.h>
#include <pico/cyw43_arch.h>
#include <pico/util/queue.h>

#include <lwip/pbuf.h>
#include <lwip/tcp.h>

#include <PicoSparkplugClient.h>
#include <Node.h>

#include "VindriktningParser.h"

#define ADDRESS BROKER_ADDRESS
#define GROUP_ID "Loungeroom"
#define NODE_ID "Vindriktning"
#define HOST_ID "Home"

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
    char clientId[40];

    Uri uri(ADDRESS);
    sprintf(clientId, "vindriktning_%ld", get_rand_32());

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

    parserConfig((Publishable *)&node);

    node.addClient<PicoSparkplugClient>(&clientOptions);

    NtpClient ntpClient(NTP_ADDRESS, 123, 3600);
    TimeManager::setInstance((TimeClient *)&ntpClient);

    startParser();
    node.enable();

    while (true)
    {
        wifi_connect();
        while (cyw43_wifi_link_status(&cyw43_state, CYW43_ITF_STA) == CYW43_LINK_JOIN)
        {
            printf("Wifi is connected, checking ntp server\n");
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
                syncParser();
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