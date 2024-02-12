#ifndef NTP_CLIENT
#define NTP_CLIENT

#include "pico/stdlib.h"

#include "lwip/dns.h"
#include "lwip/pbuf.h"
#include "lwip/udp.h"

#include <string>
#include <string.h>
#include <memory>

class NtpClient
{
private:
    ip_addr_t ntp_server_address;
    bool dns_request_sent = false;
    struct udp_pcb *ntp_pcb;
    absolute_time_t syncStamp;
    absolute_time_t ntp_test_time;
    alarm_id_t ntp_resend_alarm;

    time_t hardwareOffset = 0;

    std::string address;
    int port;
    size_t syncTime = 0;

    struct Private;

    void init();
    void request();
    void result(int status, time_t *result);
    void receive(struct udp_pcb *pcb, struct pbuf *p, const ip_addr_t *addr, u16_t port);
    void dnsFound(const char *hostname, const ip_addr_t *ipaddr);
    int64_t failed(alarm_id_t id);

protected:
public:
    NtpClient(std::string address, int port, size_t syncTime);
    ~NtpClient()
    {
    }
    static std::unique_ptr<NtpClient> create(std::string ntpServer, int port, size_t syncTime);
    void sync();
    inline bool synced()
    {
        return absolute_time_diff_us(get_absolute_time(), this->syncStamp) >= 0 && !this->dns_request_sent;
    }

    time_t getTime();
};

#endif /* NTP_CLIENT */
