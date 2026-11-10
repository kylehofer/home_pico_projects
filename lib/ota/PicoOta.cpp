#include "PicoOta.h"

#include <stdio.h>

// picowota's header has no extern "C" guard of its own.
extern "C"
{
#include "picowota/reboot.h"
}

using namespace Sparkplug;

void PicoOta::attachCallback()
{
    trigger->setCommandCallback(
        [this](__attribute__((unused)) Metric *metric, org_eclipse_tahu_protobuf_Payload_Metric *payload)
        {
            if (payload->value.boolean_value)
            {
                requestUpdate();
            }
        });
}

void PicoOta::requestUpdate()
{
    if (requested)
    {
        return;
    }

    requested = true;
    rebootAt = make_timeout_time_ms(rebootDelay);

    printf("OTA: update requested, entering bootloader in %lums\n", (unsigned long)rebootDelay);
}

bool PicoOta::isUpdateRequested() const
{
    return requested;
}

bool PicoOta::execute()
{
    if (requested && time_reached(rebootAt))
    {
        printf("OTA: rebooting into the picowota bootloader\n");
        picowota_reboot(true);
    }

    return true;
}
