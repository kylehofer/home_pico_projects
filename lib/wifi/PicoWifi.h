#ifndef LIB_WIFI_WIFI
#define LIB_WIFI_WIFI

#include "pico/cyw43_arch.h"
// #include <ExecutionTime.h>

/**
 * @brief The name this device reports to the network, before the per chip
 * suffix is appended.
 *
 * The Pico SDK defines PICO_TARGET_NAME per executable, so this picks up the
 * project automatically: pico_garage_door becomes garage-door-<chip id>. It is
 * expanded where PicoWifi is constructed, not where PicoWifi is compiled, which
 * is why it has to live in the header. Define PICO_WIFI_HOSTNAME yourself to
 * override it for a project.
 */
#ifndef PICO_WIFI_HOSTNAME
#ifdef PICO_TARGET_NAME
#define PICO_WIFI_HOSTNAME PICO_TARGET_NAME
#else
#define PICO_WIFI_HOSTNAME "pico"
#endif
#endif

/**
 * @brief Length of the hostname buffer. DHCP hostnames are capped at 63
 * characters, which is well beyond anything we generate.
 */
#define PICO_WIFI_HOSTNAME_MAX 32

/**
 * @brief How long a join gets to produce an address before it is abandoned and
 * retried. A healthy join plus DHCP takes around 3 seconds.
 */
#ifndef PICO_WIFI_JOIN_TIMEOUT_MS
#define PICO_WIFI_JOIN_TIMEOUT_MS 20000
#endif

class PicoWifi
{
private:
    char ipAddress[16] = {'\0'};
    char ssid[33] = {'\0'};
    char hostname[PICO_WIFI_HOSTNAME_MAX] = {'\0'};
    int lastResult = CYW43_LINK_DOWN;
    absolute_time_t joinDeadline = nil_time;
    // Sparkplug::Node *node;
    // NtpClient *ntpClient;
    // uint64_t lastExecutionTime = 0;

    // uint32_t getElapsed();

    bool wifiConnect();

    /**
     * @brief Copies the station interface's current address into ipAddress.
     * Only meaningful once the link is up with an address.
     */
    bool updateAddress();

    /**
     * @brief Builds the hostname from a base name and this chip's unique id,
     * and hands it to lwip for the DHCP request.
     *
     * @param name The base name, usually the executable name.
     */
    void configureHostname(const char *name);

protected:
public:
    char *getAddress();
    char *getSsid();
    int32_t getRssi();

    /**
     * @brief The name reported to the network, as
     * <project>-<chip id>, for example garage-door-e66141.
     */
    const char *getHostname() const;

    /**
     * @brief Brings up the WiFi chip and sets the network hostname.
     *
     * @param name The base name to build the hostname from. Defaults to the
     *             executable name, with any leading "pico_" removed.
     */
    // Sparkplug::Node *node, NtpClient *ntpClient
    PicoWifi(const char *name = PICO_WIFI_HOSTNAME);
    ~PicoWifi();

    bool execute();
};

#endif /* LIB_WIFI_CONNECTOR */
