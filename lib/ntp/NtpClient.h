#ifndef NTP_CLIENT
#define NTP_CLIENT

#include "pico/stdlib.h"

#include "lwip/dns.h"
#include "lwip/pbuf.h"
#include "lwip/udp.h"

#include <string>
#include <string.h>
#include <memory>
#include "utils/TimeManager.h"

class NtpClient : Sparkplug::TimeClient
{
private:
    ip_addr_t ntp_server_address = {.addr = IPADDR_NONE};
    bool dns_request_sent = false;
    struct udp_pcb *ntp_pcb = nullptr;
    absolute_time_t syncStamp;

    time_t hardwareOffset = 0;

    std::string address;
    int port;
    size_t syncTime = 0;

    struct Private;

    void request();
    void result(int status, time_t *result);
    void receive(struct udp_pcb *pcb, struct pbuf *p, const ip_addr_t *addr, u16_t port);
    void dnsFound(const char *hostname, const ip_addr_t *ipaddr);
    bool isValidIPv4(std::string address, ip_addr_t &result);
    int64_t failed(alarm_id_t id);
    void init();

    /**
     * @brief How long to wait for a reply before asking again: short until the
     * first sync, longer after that.
     */
    uint32_t resendTime();

protected:
public:
    NtpClient(std::string address, int port, size_t syncTime);
    ~NtpClient()
    {
    }
    static std::unique_ptr<NtpClient> create(std::string ntpServer, int port, size_t syncTime);
    bool sync();
    inline bool synced()
    {
        return hardwareOffset > 0;
    }

    virtual time_t getTime() override;
};

#endif /* NTP_CLIENT */
