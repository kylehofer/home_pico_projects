/*
 * File: PicoTcpClient.h
 * Project: pico_tcp_client
 * Created Date: Sunday December 25th 2022
 * Author: Kyle Hofer
 *
 * MIT License
 *
 * Copyright (c) 2022 Kyle Hofer
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is furnished to do
 * so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * HISTORY:
 */

#ifndef PICOTCPCLIENT
#define PICOTCPCLIENT

#include <stddef.h>
#include <stdint.h>

#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"

// Pulls in lwipopts.h and lwIP's defaults (TCP_SND_BUF, PBUF_POOL_SIZE, ...)
#include "lwip/opt.h"
#include "lwip/pbuf.h"
#include "lwip/tcp.h"

#include "SocketClient.h"
// From cpp_mqtt_client
#include "TransmitBuffer.h"
#include "utils/TransmitRing.h"

#define BASE_ERROR -1

/**
 * Bytes of outgoing data held until the broker acknowledges them. lwIP sends
 * straight out of this memory, so it is the only transmit storage the client
 * needs. Sized to lwIP's send window (TCP_SND_BUF) by default, so the whole
 * window can be in flight; it must be at least the largest packet plus
 * CPP_MQTT_TRANSMIT_RESERVE.
 */
#ifndef PICO_TCP_TRANSMIT_BUFFER_SIZE
#define PICO_TCP_TRANSMIT_BUFFER_SIZE TCP_SND_BUF
#endif

/**
 * Most received pbufs held for the application at once. Received data is kept
 * in lwIP's pool pbufs until read, and every segment takes a whole pool pbuf
 * however small it is, so this stops a burst of small packets draining the pool
 * that all incoming traffic (ACKs included) needs. Past the limit lwIP keeps
 * the data and retries, which pauses the broker until it is read.
 */
#ifndef PICO_TCP_RECEIVE_PBUF_LIMIT
#define PICO_TCP_RECEIVE_PBUF_LIMIT (PBUF_POOL_SIZE / 2)
#endif

/**
 * Seconds a closed connection may spend delivering its last data (such as an
 * MQTT DISCONNECT) before it is reset. The next connect() waits for this.
 */
#ifndef PICO_TCP_CLOSE_TIMEOUT_S
#define PICO_TCP_CLOSE_TIMEOUT_S 10
#endif

/**
 * Default for setNoDelay(). 1 turns Nagle's algorithm off, so each packet is sent
 * as soon as it is committed instead of waiting for earlier data to be
 * acknowledged. 0 leaves Nagle on, which batches small packets into fewer
 * segments (fewer transmissions, more latency).
 */
#ifndef PICO_TCP_NO_DELAY
#define PICO_TCP_NO_DELAY 1
#endif

/**
 * lwIP only sends from caller memory when LWIP_NETIF_TX_SINGLE_PBUF is 0. With
 * it set to 1, tcp_write() always copies into lwIP's heap; the client still
 * works, but frees ring space as soon as lwIP has copied it.
 */
#define PICO_TCP_ZERO_COPY_TRANSMIT (!LWIP_NETIF_TX_SINGLE_PBUF)

/**
 * @brief A TCP client for the Pico W on lwIP's raw API that never allocates.
 *
 * Sending: implements TransmitBuffer, so MqttClient encodes each packet
 * straight into a fixed ring inside this object. lwIP sends from the ring
 * without copying (tcp_write without TCP_WRITE_FLAG_COPY) and ring space is
 * freed as the broker acknowledges it. Needs LWIP_NETIF_TX_SINGLE_PBUF 0 and
 * MEMP_NUM_PBUF >= TCP_SND_QUEUELEN in lwipopts.h; see
 * PICO_TCP_ZERO_COPY_TRANSMIT.
 *
 * Receiving: keeps the pbufs lwIP delivers instead of copying them into a
 * buffer, and read() copies straight from them into the caller's memory. Data
 * is acknowledged to lwIP only once read, so the TCP window paces the broker
 * to what the application consumes.
 *
 * Connecting: takes an IP address or, with LWIP_DNS, a host name. Either way
 * connect() only starts the attempt; connected() turns true once it is up.
 *
 * Closing: stop() and a broker FIN close gracefully. Data already committed is
 * delivered and the ring is not reused until the broker has acknowledged it,
 * so connect() returns BASE_ERROR until then (at most
 * PICO_TCP_CLOSE_TIMEOUT_S).
 *
 * Keep it static, global or a member of a long-lived object: it holds the
 * whole transmit ring, and a DNS lookup in progress cannot be cancelled, so the
 * object must outlive it. MqttClient finds its TransmitBuffer side from the
 * type, so passing it once is enough:
 *   static PicoTcpClient tcp;
 *   MqttClient mqtt(&tcp);
 */
class PicoTcpClient : public Client, public TransmitBuffer
{
private:
    struct tcp_pcb *activeBlock = NULL;
    // A closed connection still delivering data from the ring
    struct tcp_pcb *closingBlock = NULL;
    ip_addr_t address;
    uint16_t port = 0;
    bool isConnected = false;
    // A DNS lookup for connect() is in progress
    bool resolving = false;
    // Identifies the host name being resolved, so a stale answer is ignored
    uint32_t resolvingHash = 0;
    // The broker sent FIN while unread data was held
    bool remoteClosed = false;
    // tcp_close() has been called on closingBlock
    bool finQueued = false;
    uint8_t closePolls = 0;
    bool noDelay = PICO_TCP_NO_DELAY;

    CppMqtt::TransmitRing<PICO_TCP_TRANSMIT_BUFFER_SIZE> transmitRing;
    // Received pbufs not yet read; all of it is still unacknowledged to lwIP
    struct pbuf *receiveChain = NULL;

    // lwIP callbacks; arg is the PicoTcpClient. They run with the lwIP lock held
    static err_t lwipConnected(void *arg, struct tcp_pcb *controlBlock, err_t errorCode);
    static err_t lwipReceived(void *arg, struct tcp_pcb *controlBlock, struct pbuf *payload, err_t errorCode);
    static err_t lwipSent(void *arg, struct tcp_pcb *controlBlock, u16_t length);
    static err_t lwipPoll(void *arg, struct tcp_pcb *controlBlock);
    static void lwipError(void *arg, err_t errorCode);
    static void lwipClosingError(void *arg, err_t errorCode);
    static void lwipDnsFound(const char *hostname, const ip_addr_t *found, void *arg);

    err_t onReceived(struct tcp_pcb *controlBlock, struct pbuf *payload, err_t errorCode);
    err_t onSent(struct tcp_pcb *controlBlock, u16_t length);
    err_t onPoll(struct tcp_pcb *controlBlock);
    void onError();
    void onClosingError();
    void onDnsFound(const char *hostname, const ip_addr_t *found);

    // The helpers below are called with the lwIP lock held. Those returning
    // bool return true when they aborted the connection they worked on, which
    // an lwIP callback for that connection must report as ERR_ABRT.
    bool startConnection();
    void sendPending(struct tcp_pcb *controlBlock);
    size_t unsentLength() const;
    bool beginClose();
    bool advanceClose();
    void abortClosing();
    void dropReceived(struct tcp_pcb *controlBlock);
    void applyNoDelay(struct tcp_pcb *controlBlock);
    static void detach(struct tcp_pcb *controlBlock);
    static uint32_t hashName(const char *name);

public:
    PicoTcpClient();
    ~PicoTcpClient();
    PicoTcpClient(const PicoTcpClient &) = delete;
    PicoTcpClient &operator=(const PicoTcpClient &) = delete;

    // Client
    /**
     * @param host An IP address, or a host name when LWIP_DNS is enabled
     * @return int 1 once the connection attempt has started (connected() turns
     * true when it is established), BASE_ERROR otherwise
     */
    virtual int connect(const char *host, uint16_t port) override;
    virtual size_t write(uint8_t) override;
    /**
     * @brief Copies into the transmit ring and sends from there. All or
     * nothing: returns 0 if the ring cannot take every byte.
     */
    virtual size_t write(const void *buffer, size_t length) override;
    virtual int available() override;
    virtual int read(void *buffer, size_t length) override;
    virtual void stop() override;
    virtual uint8_t connected() override;
    virtual void sync() override;

    /**
     * @brief Turns Nagle's algorithm off (true) or on (false). Applies to the
     * current connection immediately and to later ones. Defaults to
     * PICO_TCP_NO_DELAY. Matches the Arduino WiFiClient::setNoDelay() API.
     */
    void setNoDelay(bool value);
    bool getNoDelay();

    // TransmitBuffer
    virtual size_t writable() override;
    virtual uint8_t *writePointer(size_t offset, size_t &contiguous) override;
    virtual bool commit(size_t length) override;
};

#endif /* PICOTCPCLIENT */
