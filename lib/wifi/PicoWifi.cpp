#include "PicoWifi.h"

#include <stdio.h>
#include <string.h>

#include "pico/cyw43_arch.h"
#include "pico/unique_id.h"
#include "lwip/pbuf.h"
#include "lwip/tcp.h"
#include "lwip/inet.h"
#include "lwip/netif.h"

bool PicoWifi::updateAddress()
{
    char current[sizeof(ipAddress)];

    // Read from the station interface rather than netif_default, so this
    // cannot pick up a different interface.
    ip4addr_ntoa_r(netif_ip4_addr(&cyw43_state.netif[CYW43_ITF_STA]), current, sizeof(current));

    if (strcmp(current, ipAddress) != 0)
    {
        memcpy(ipAddress, current, sizeof(ipAddress));
        printf("WIFI: address is %s\n", ipAddress);
    }

    return true;
}

bool PicoWifi::execute()
{

#if PICO_CYW43_ARCH_POLL
    cyw43_arch_poll(); // Essential to let lwIP process DHCP packets!
#endif

    // cyw43_wifi_link_status() reports CYW43_LINK_JOIN as soon as the radio
    // has associated, which is before DHCP has handed us an address. The tcpip
    // status only reports CYW43_LINK_UP once there is an address to read.
    int ret = cyw43_tcpip_link_status(&cyw43_state, CYW43_ITF_STA);

    if (ret != lastResult)
    {
        lastResult = ret;
        if (ret == CYW43_LINK_UP)
        {
            memcpy(ssid, (void *)WIFI_SSID, sizeof(WIFI_SSID));
        }
        else
        {
            ipAddress[0] = '\0';
        }
    }

    switch (ret)
    {
    case CYW43_LINK_UP:
        // Refreshed every time, so a new lease is picked up too.
        return updateAddress();

    case CYW43_LINK_JOIN:
    case CYW43_LINK_NOIP:
        // A join is in progress (JOIN is reported from the moment it starts,
        // not once associated), or we are associated and waiting on DHCP.
        // Starting another join now would throw that progress away, so give it
        // until the deadline.
        if (!time_reached(joinDeadline))
        {
            return false;
        }

        // Joins can stall indefinitely, which is reliably the case on the first
        // join after the Pico was reset while associated: the access point
        // still holds the old association. Abandon it and start over.
        printf("WIFI: no address after %d ms (%s), rejoining\n",
               PICO_WIFI_JOIN_TIMEOUT_MS, ret == CYW43_LINK_JOIN ? "joining" : "no ip");
        cyw43_wifi_leave(&cyw43_state, CYW43_ITF_STA);

        // The driver only takes the link down on a disassociate event, which
        // never arrives if the join stalled. Without this the next link up is a
        // no-op, so DHCP is not restarted and stays on its existing, ever
        // longer, retry backoff.
        netif_set_link_down(&cyw43_state.netif[CYW43_ITF_STA]);
        break;

    default:
        break;
    }

    joinDeadline = make_timeout_time_ms(PICO_WIFI_JOIN_TIMEOUT_MS);

    // This blocks for at most 3 seconds. If it times out mid join or before
    // DHCP completes, the join carries on in the background and the case above
    // waits for it, up to joinDeadline.
    if (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 3000) != 0)
    {
        return false;
    }

    lastResult = CYW43_LINK_UP;
    memcpy(ssid, (void *)WIFI_SSID, sizeof(WIFI_SSID));
    return updateAddress();
}

void PicoWifi::configureHostname(const char *name)
{
    static const char PREFIX[] = "pico_";
    // Room for the "-xxxxxx" chip id suffix and the terminator.
    static const size_t SUFFIX_LENGTH = 7;

    if (name == NULL)
    {
        name = "pico";
    }

    // The executables are all named pico_<project>, and the prefix is noise on
    // a network where every one of these is a Pico.
    if (strncmp(name, PREFIX, sizeof(PREFIX) - 1) == 0 && name[sizeof(PREFIX) - 1] != '\0')
    {
        name += sizeof(PREFIX) - 1;
    }

    size_t limit = sizeof(hostname) - SUFFIX_LENGTH - 1;
    size_t length = 0;

    for (size_t i = 0; name[i] != '\0' && length < limit; i++)
    {
        char character = name[i];

        if (character >= 'A' && character <= 'Z')
        {
            character = character - 'A' + 'a';
        }

        if ((character >= 'a' && character <= 'z') || (character >= '0' && character <= '9'))
        {
            hostname[length++] = character;
        }
        else if (length > 0 && hostname[length - 1] != '-')
        {
            // Underscores and anything else not allowed in a hostname become
            // separators, without ever doubling one up.
            hostname[length++] = '-';
        }
    }

    // A hostname cannot end on a separator.
    while (length > 0 && hostname[length - 1] == '-')
    {
        length--;
    }

    if (length == 0)
    {
        memcpy(hostname, "pico", 4);
        length = 4;
    }

    // The unique id is the flash chip's 64 bit id. The low three bytes are
    // plenty to tell our handful of devices apart, and keep the name readable.
    pico_unique_board_id_t boardId;
    pico_get_unique_board_id(&boardId);

    snprintf(
        hostname + length, sizeof(hostname) - length, "-%02x%02x%02x",
        boardId.id[PICO_UNIQUE_BOARD_ID_SIZE_BYTES - 3],
        boardId.id[PICO_UNIQUE_BOARD_ID_SIZE_BYTES - 2],
        boardId.id[PICO_UNIQUE_BOARD_ID_SIZE_BYTES - 1]);

#if LWIP_NETIF_HOSTNAME
    // cyw43 sets this to CYW43_HOST_NAME while bringing the interface up, so
    // this has to happen after the init above. It stores the pointer rather
    // than a copy, hence the buffer living in this object: the interface is
    // torn down in the destructor, so it cannot outlive the name.
    netif_set_hostname(&cyw43_state.netif[CYW43_ITF_STA], hostname);
#endif

    printf("WIFI: hostname is %s\n", hostname);
}

PicoWifi::PicoWifi(const char *name)
{
    if (cyw43_arch_init_with_country(CYW43_COUNTRY_AUSTRALIA))
    {
        printf("failed to initialise\n");
    }
    else
    {
        cyw43_arch_enable_sta_mode();
        configureHostname(name);
    }
}

// PicoWifi::PicoWifi(Sparkplug::Node *node, NtpClient *ntpClient) : node(node), ntpClient(ntpClient)
// {
//     if (cyw43_arch_init_with_country(CYW43_COUNTRY_AUSTRALIA))
//     {
//         printf("failed to initialise\n");
//     }
//     else
//     {
//         cyw43_arch_enable_sta_mode();
//     }
// }

PicoWifi::~PicoWifi()
{
    cyw43_arch_deinit();
}

char *PicoWifi::getAddress()
{
    return this->ipAddress;
}

char *PicoWifi::getSsid()
{
    return this->ssid;
}

const char *PicoWifi::getHostname() const
{
    return this->hostname;
}

int32_t PicoWifi::getRssi()
{
    int32_t rssi = 0;
    cyw43_wifi_get_rssi(&cyw43_state, &rssi);
    return rssi;
}
