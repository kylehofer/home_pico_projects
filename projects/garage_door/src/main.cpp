#include <time.h>

#include <PicoSparkplugClient.h>

#include <Node.h>
#include <Device.h>
#include <metrics/simple/Int32Metric.h>
#include <metrics/simple/StringMetric.h>
#include <properties/complex/PropertySet.h>
#include <properties/simple/StringProperty.h>

#include <NtpClient.h>
#include <utils/TimeManager.h>

#include <PicoWifi.h>
#include <ElapsedTime.h>
#include <DependencyExecutor.h>
#include <PicoOta.h>

#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"

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

void sparkplug()
{
    char clientId[40];

    Uri uri(ADDRESS);
    sprintf(clientId, "garage_door_%lu", get_rand_32());

    DoorControl doorControl;
    DoorControl *doorControlPtr = &doorControl;
    door_control_initialize(doorControlPtr, 0, 4096);

    NodeOptions nodeOptions = {
        GROUP_ID,
        NODE_ID,
        HOST_ID,
        5000,
        NODE_CONTROL_REBIRTH};

    ClientOptions clientOptions = {
        .address = uri,
        .clientId = clientId,
        .username = NULL,
        .password = NULL,
        .connectTimeout = 5000,
        .keepAliveInterval = 5};

    Node node = Node(&nodeOptions);

    PicoOta ota(&node);

    std::shared_ptr<BooleanMetric> trigger;
    std::shared_ptr<StringMetric> address;
    std::shared_ptr<StringMetric> ssid;
    std::shared_ptr<Int32Metric> rssi;

    node.addMetrics({
        trigger = BooleanMetric::create("trigger", false),
        address = StringMetric::create("ipAddress", {'\0'}),
        ssid = StringMetric::create("ssid", {'\0'}),
        rssi = Int32Metric::create("rssi", 0),
    });

    rssi->addProperty(StringProperty::create("unit", "dBm"));

    trigger->setCommandCallback(
        [&doorControlPtr](__attribute__((unused)) Metric *metric, org_eclipse_tahu_protobuf_Payload_Metric *payload)
        {
            if (!payload->value.boolean_value)
            {
                return;
            }
            door_control_trigger(doorControlPtr);
        });

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
        [&node, &timeHandler, &address, &wifi, &rssi, &ssid, &doorControlPtr]()
        {
            uint32_t elapsedTime = timeHandler.elapsed();
            if (elapsedTime > 0)
            {
                address->setValue(wifi.getAddress());
                ssid->setValue(wifi.getSsid());
                rssi->setValue(wifi.getRssi());

                node.execute(elapsedTime);
            }

            door_control_execute(doorControlPtr);

            return true;
        },
        std::bind(&PicoOta::execute, &ota),
    });

    while (true)
    {

        if (executor.execute())
        {
        }
    }
}

int main()
{
    stdio_init_all();

    sparkplug();

    cyw43_arch_deinit();

    return 0;
}