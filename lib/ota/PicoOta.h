#ifndef LIB_OTA_PICOOTA
#define LIB_OTA_PICOOTA

#include <memory>

#include "Node.h"
#include "Device.h"
#include "metrics/simple/BooleanMetric.h"
#include "pico/time.h"

/**
 * @brief Over the air firmware updates, backed by the picowota bootloader.
 *
 * Registers a writable boolean metric on the supplied node or device. Writing
 * true to that metric reboots into the picowota bootloader, which joins the
 * WiFi and waits on TCP 4242 for a replacement application image:
 *
 *     tools/flash.sh --ota <device address> <project>
 *
 * The reboot is deferred rather than taken inside the command callback, so the
 * command acknowledgement still reaches the broker. Add execute() to the end of
 * the dependency chain, after the call to Node::execute(), and the reboot is
 * taken on a later pass once the delay has elapsed.
 *
 * If the bootloader is not present (see PICO_OTA in the top level
 * CMakeLists.txt), writing the metric reboots back into the application, which
 * is harmless but achieves nothing.
 */
class PicoOta
{
private:
    std::shared_ptr<Sparkplug::BooleanMetric> trigger;
    uint32_t rebootDelay;
    absolute_time_t rebootAt;
    bool requested = false;

    /**
     * @brief Hooks the command callback up to the registered metric.
     */
    void attachCallback();

public:
    /**
     * @brief The default name of the metric used to request an update.
     */
    static constexpr const char *DEFAULT_METRIC_NAME = "enterOta";

    /**
     * @brief Construct an OTA handler and register its command metric.
     *
     * Templated so it takes either a Node or a Device; both expose addMetrics,
     * but Node inherits Publishable privately, so a Publishable parameter here
     * would force every caller into a cast.
     *
     * @param publishable The node or device to add the metric to.
     * @param name The name of the metric used to request an update.
     * @param delayMs How long to wait after a request before rebooting, giving
     *                the command acknowledgement time to be published.
     */
    template <typename TPublishable>
    PicoOta(TPublishable *publishable, const char *name = DEFAULT_METRIC_NAME, uint32_t delayMs = 1000)
        : rebootDelay(delayMs), rebootAt(nil_time)
    {
        publishable->addMetrics({trigger = Sparkplug::BooleanMetric::create(name, false)});
        attachCallback();
    }

    /**
     * @brief Request a reboot into the bootloader. Repeat requests are ignored.
     */
    void requestUpdate();

    /**
     * @brief Whether a reboot into the bootloader is pending.
     */
    bool isUpdateRequested() const;

    /**
     * @brief Takes a pending reboot once its delay has elapsed. Intended for a
     * DependencyExecutor chain, so it returns true to let the chain continue.
     *
     * @return true always; it does not return at all if it reboots.
     */
    bool execute();
};

#endif /* LIB_OTA_PICOOTA */
