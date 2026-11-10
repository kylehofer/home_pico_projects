
#include "NtpClient.h"

#include <iostream>
#include <iomanip>
#include <cstdint>
#include "hardware/timer.h"
#include "hardware/sync.h"

#include "pico/cyw43_arch.h"

#define NTP_MSG_LEN 48
#define NTP_PORT 123
#define NTP_DELTA 2208988800 // seconds between 1 Jan 1900 and 1 Jan 1970
#define SECONDS_TO_MS 1000
#define FAILED_SYNC_DELAY_SECONDS 5
#define NTP_RESEND_TIME (10 * SECONDS_TO_MS)
// Until the first sync nothing downstream can run, so a lost request or reply
// is retried quickly. Right after joining the network the first few packets
// regularly go unanswered.
#define NTP_INITIAL_RESEND_TIME (2 * SECONDS_TO_MS)

struct NtpClient::Private
{
    static void dnsFound(const char *hostname, const ip_addr_t *ipaddr, void *arg)
    {
        NtpClient *client = (NtpClient *)arg;
        return client->dnsFound(hostname, ipaddr);
    }

    static int64_t failed(alarm_id_t id, void *user_data)
    {
        NtpClient *client = (NtpClient *)user_data;
        return client->failed(id);
    }

    static void receive(void *arg, struct udp_pcb *pcb, struct pbuf *p, const ip_addr_t *addr, u16_t port)
    {
        NtpClient *client = (NtpClient *)arg;
        return client->receive(pcb, p, addr, port);
    }
};

bool NtpClient::sync()
{
    if (ntp_pcb == nullptr)
    {
        init();
    }

    // While a lookup is outstanding, dnsFound() takes it from here. Calling
    // dns_gethostbyname() again would queue another request for the same name
    // on every pass, until lwIP's table fills up, and each one would send its
    // own NTP request when it resolved.

    if (ntp_pcb != nullptr && !dns_request_sent && time_reached(this->syncStamp))
    {
        if (ntp_server_address.addr == IPADDR_NONE)
        {
#if PICO_CYW43_ARCH_THREADSAFE_BACKGROUND
            // cyw43_arch_lwip_begin/end should be used around calls into lwIP to ensure correct locking.
            // You can omit them if you are in a callback from lwIP. Note that when using pico_cyw_arch_poll
            // these calls are a no-op and can be omitted, but it is a good practice to use them in
            // case you switch the cyw43_arch type later.
            cyw43_arch_lwip_begin();
#endif

            printf("dns_gethostbyname %s\n", address.c_str());
            int err = dns_gethostbyname(address.c_str(), &ntp_server_address, Private::dnsFound, this);
#if PICO_CYW43_ARCH_THREADSAFE_BACKGROUND
            cyw43_arch_lwip_end();
#endif
            if (err == ERR_OK)
            {
                request(); // Cached result
            }
            else if (err == ERR_INPROGRESS)
            {
                // Expect a callback, with a NULL address if the lookup fails.
                dns_request_sent = true;
            }
            else
            {
                result(-1, NULL);
            }
        }
        else
        {
            printf("Cached result\n");
            request(); // Cached result
        }
#if PICO_CYW43_ARCH_POLL
        cyw43_arch_poll();
#endif
    }

    return synced();
}

time_t NtpClient::getTime()
{
    return ((time_t)us_to_ms(time_us_64())) + hardwareOffset;
}

void NtpClient::request()
{
    // cyw43_arch_lwip_begin/end should be used around calls into lwIP to ensure correct locking.
    // You can omit them if you are in a callback from lwIP. Note that when using pico_cyw_arch_poll
    // these calls are a no-op and can be omitted, but it is a good practice to use them in
    // case you switch the cyw43_arch type later.

#if PICO_CYW43_ARCH_THREADSAFE_BACKGROUND
    cyw43_arch_lwip_begin();
#endif
    // Whether or not this gets out, try again after the resend time. A reply
    // pushes the next attempt out to the full sync interval.
    syncStamp = make_timeout_time_ms(resendTime());

    struct pbuf *p = pbuf_alloc(PBUF_TRANSPORT, NTP_MSG_LEN, PBUF_RAM);
    if (p != nullptr)
    {
        uint8_t *req = (uint8_t *)p->payload;
        memset(req, 0, NTP_MSG_LEN);
        req[0] = 0x1b;
        udp_sendto(ntp_pcb, p, &ntp_server_address, port);
        pbuf_free(p);
    }
#if PICO_CYW43_ARCH_THREADSAFE_BACKGROUND
    cyw43_arch_lwip_end();
#endif
}

void NtpClient::result(int status, time_t *result)
{
    if (status == 0 && result != nullptr)
    {
        if (!synced())
        {
            printf("NTP: synced with %s\n", address.c_str());
        }
        hardwareOffset = (*result * 1000) - ((time_t)us_to_ms(time_us_64()));
        syncStamp = make_timeout_time_ms(syncTime * SECONDS_TO_MS);
    }
    else
    {
        syncStamp = make_timeout_time_ms(resendTime());
    }

    dns_request_sent = false;
}

void NtpClient::receive(struct udp_pcb *pcb, struct pbuf *p, const ip_addr_t *addr, u16_t port)
{
    uint8_t mode = pbuf_get_at(p, 0) & 0x7;
    uint8_t stratum = pbuf_get_at(p, 1);

    // Check the result, void *arg
    if (ip_addr_cmp(addr, &ntp_server_address) && port == this->port && p->tot_len == NTP_MSG_LEN &&
        mode == 0x4 && stratum != 0)
    {
        uint8_t seconds_buf[4] = {0};
        pbuf_copy_partial(p, seconds_buf, sizeof(seconds_buf), 40);
        uint32_t seconds_since_1900 = seconds_buf[0] << 24 | seconds_buf[1] << 16 | seconds_buf[2] << 8 | seconds_buf[3];
        uint32_t seconds_since_1970 = seconds_since_1900 - NTP_DELTA;
        time_t epoch = seconds_since_1970;
        result(0, &epoch);
    }
    else
    {
        result(-1, NULL);
    }
    pbuf_free(p);
}

void NtpClient::dnsFound(const char *hostname, const ip_addr_t *ipaddr)
{
    (void)hostname;
    if (ipaddr)
    {
        ntp_server_address = *ipaddr;
        printf("ntp address %s\n", ip4addr_ntoa(ipaddr));
        request();
    }
    else
    {
        result(-1, NULL);
    }
}

bool NtpClient::isValidIPv4(std::string address, ip_addr_t &result)
{
    int a, b, c, d;
    if (sscanf(address.c_str(), "%d.%d.%d.%d", &a, &b, &c, &d) == 4)
    {
        result.addr = d << 24 | c << 16 | b << 8 | a;
        return true;
    }
    return false;
}

int64_t NtpClient::failed(alarm_id_t id)
{
    result(-1, NULL);
    return 0;
}
uint32_t NtpClient::resendTime()
{
    return synced() ? NTP_RESEND_TIME : NTP_INITIAL_RESEND_TIME;
}

std::unique_ptr<NtpClient> NtpClient::create(std::string ntpServer, int port, size_t syncTime)
{
    return std::unique_ptr<NtpClient>(new NtpClient(ntpServer, port, syncTime));
}

NtpClient::NtpClient(std::string address, int port, size_t syncTime) : address(address), port(port), syncTime(syncTime)
{
    syncStamp = get_absolute_time();
}

void NtpClient::init()
{
    ntp_pcb = udp_new_ip_type(IPADDR_TYPE_ANY);
    if (!ntp_pcb)
    {
        // printf("failed to create pcb\n");
    }
    // isValidIPv4(address, ntp_server_address);
    udp_recv(ntp_pcb, Private::receive, this);
}
