#include <string.h>
#include <time.h>

#include <PicoSparkplugClient.h>

#include <Node.h>
#include <Device.h>
#include <metrics/simple/UInt8Metric.h>
#include <metrics/simple/Int32Metric.h>
#include <metrics/simple/DateTimeMetric.h>

#include <PicoWifi.h>
#include <ElapsedTime.h>
#include <DependencyExecutor.h>
#include <PicoOta.h>

#include <NtpClient.h>
#include "Bed.h"

#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "pico/multicore.h"
#include "pico/util/queue.h"
#include "hardware/gpio.h"

#include "lwip/pbuf.h"
#include "lwip/tcp.h"

#define ADDRESS BROKER_ADDRESS
#define GROUP_ID "Garden"
#define NODE_ID "Bed"
#define HOST_ID "Home"

#define MAX_RETRIES 5000

void sparkplug()
{
    char clientId[40];

    Uri uri(ADDRESS);
    sprintf(clientId, "garden_bed_%lu", get_rand_32());

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

    Bed bed(&node);

    PicoOta ota(&node);

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
        [&node, &timeHandler]()
        {
            uint32_t elapsedTime = timeHandler.elapsed();
            if (elapsedTime > 0)
            {
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
        bed.sync(TimeManager::getTime());
    }
}

int main()
{
    stdio_init_all();

    sparkplug();

    cyw43_arch_deinit();

    return 0;
}