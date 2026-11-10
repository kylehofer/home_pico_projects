/*
 * File: PicoTcpClient.cpp
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

#include "PicoTcpClient.h"

#include <string.h>
#include <stdio.h>

#if LWIP_DNS
#include "lwip/dns.h"
#endif

// Build with -DPICO_TCP_DEBUGGING to print connection events
#if defined(PICO_TCP_DEBUGGING) && PICO_TCP_DEBUGGING
#define PICO_TCP_LOG(format, ...) printf("PicoTcpClient: " format, ##__VA_ARGS__)
#else
#define PICO_TCP_LOG(format, ...) \
    do                            \
    {                             \
    } while (0)
#endif

// tcp_poll() interval, in lwIP's 500 ms coarse timer ticks. Retries sends that
// did not fit and times out closing connections.
#define POLL_INTERVAL 2
#define CLOSE_POLL_LIMIT ((PICO_TCP_CLOSE_TIMEOUT_S * 1000) / (POLL_INTERVAL * 500))

#if PICO_TCP_ZERO_COPY_TRANSMIT
// lwIP references the ring rather than copying it
#define WRITE_FLAGS 0
#else
#define WRITE_FLAGS TCP_WRITE_FLAG_COPY
#endif

#if PICO_TCP_ZERO_COPY_TRANSMIT && !MEMP_MEM_MALLOC && (MEMP_NUM_PBUF < TCP_SND_QUEUELEN)
#warning "PicoTcpClient: MEMP_NUM_PBUF < TCP_SND_QUEUELEN; no-copy sends will run out of pbufs before the send window is full"
#endif

static_assert(PICO_TCP_RECEIVE_PBUF_LIMIT >= 1, "PICO_TCP_RECEIVE_PBUF_LIMIT must be at least 1");
static_assert(CLOSE_POLL_LIMIT >= 1 && CLOSE_POLL_LIMIT <= 255, "PICO_TCP_CLOSE_TIMEOUT_S is out of range");
// Held data is acknowledged with tcp_recved() and measured by a pbuf's u16 tot_len
static_assert(TCP_WND <= 0xFFFF, "PicoTcpClient needs TCP_WND to fit in 16 bits");

/*
 * lwIP callbacks. Every callback is cleared before arg stops being valid, but
 * a NULL arg is still ignored.
 */

err_t PicoTcpClient::lwipConnected(void *arg, struct tcp_pcb *controlBlock, err_t errorCode)
{
    PicoTcpClient *client = (PicoTcpClient *)arg;
    if (client == NULL || controlBlock != client->activeBlock)
    {
        return ERR_OK;
    }
    // lwIP reports failures through the error callback; errorCode is ERR_OK
    (void)errorCode;
    PICO_TCP_LOG("Connected\n");
    client->isConnected = true;
    client->applyNoDelay(controlBlock);
    return ERR_OK;
}

err_t PicoTcpClient::lwipReceived(void *arg, struct tcp_pcb *controlBlock, struct pbuf *payload, err_t errorCode)
{
    PicoTcpClient *client = (PicoTcpClient *)arg;
    if (client == NULL)
    {
        if (payload != NULL)
        {
            tcp_recved(controlBlock, payload->tot_len);
            pbuf_free(payload);
        }
        return ERR_OK;
    }
    return client->onReceived(controlBlock, payload, errorCode);
}

err_t PicoTcpClient::lwipSent(void *arg, struct tcp_pcb *controlBlock, u16_t length)
{
    PicoTcpClient *client = (PicoTcpClient *)arg;
    return (client != NULL) ? client->onSent(controlBlock, length) : (err_t)ERR_OK;
}

err_t PicoTcpClient::lwipPoll(void *arg, struct tcp_pcb *controlBlock)
{
    PicoTcpClient *client = (PicoTcpClient *)arg;
    return (client != NULL) ? client->onPoll(controlBlock) : (err_t)ERR_OK;
}

void PicoTcpClient::lwipError(void *arg, err_t errorCode)
{
    // lwIP has already freed the connection
    PICO_TCP_LOG("Connection error: %d\n", errorCode);
    (void)errorCode;
    if (arg != NULL)
    {
        ((PicoTcpClient *)arg)->onError();
    }
}

void PicoTcpClient::lwipClosingError(void *arg, err_t errorCode)
{
    // The closing connection was reset or timed out; lwIP has freed it
    PICO_TCP_LOG("Closing connection error: %d\n", errorCode);
    (void)errorCode;
    if (arg != NULL)
    {
        ((PicoTcpClient *)arg)->onClosingError();
    }
}

void PicoTcpClient::lwipDnsFound(const char *hostname, const ip_addr_t *found, void *arg)
{
    if (arg != NULL)
    {
        ((PicoTcpClient *)arg)->onDnsFound(hostname, found);
    }
}

PicoTcpClient::PicoTcpClient()
{
    ip_addr_set_zero(&address);
}

PicoTcpClient::~PicoTcpClient()
{
    // lwIP must not touch the ring or this object once it is gone, so both
    // connections are aborted rather than closed
    cyw43_arch_lwip_begin();
    resolving = false;
    if (activeBlock != NULL)
    {
        struct tcp_pcb *controlBlock = activeBlock;
        activeBlock = NULL;
        dropReceived(NULL);
        detach(controlBlock);
        tcp_abort(controlBlock);
    }
    if (closingBlock != NULL)
    {
        abortClosing();
    }
    dropReceived(NULL);
    cyw43_arch_lwip_end();
}

/**
 * Clears every callback so lwIP no longer calls into this object.
 */
void PicoTcpClient::detach(struct tcp_pcb *controlBlock)
{
    tcp_arg(controlBlock, NULL);
    tcp_recv(controlBlock, NULL);
    tcp_sent(controlBlock, NULL);
    tcp_poll(controlBlock, NULL, 0);
    tcp_err(controlBlock, NULL);
}

/**
 * FNV-1a, to match a DNS answer to the request without keeping a copy of the
 * name.
 */
uint32_t PicoTcpClient::hashName(const char *name)
{
    uint32_t hash = 2166136261u;
    for (; name != NULL && *name != '\0'; name++)
    {
        // DNS names are case insensitive
        char character = *name;
        if (character >= 'A' && character <= 'Z')
        {
            character = (char)(character - 'A' + 'a');
        }
        hash = (hash ^ (uint8_t)character) * 16777619u;
    }
    return hash;
}

size_t PicoTcpClient::unsentLength() const
{
    return transmitRing.pending() - transmitRing.unacknowledged();
}

/**
 * Hands committed bytes from the ring to lwIP, as far as its send buffer and
 * segment queue allow. Whatever is left is retried when the broker
 * acknowledges data (onSent) or on the poll timer.
 */
void PicoTcpClient::sendPending(struct tcp_pcb *controlBlock)
{
    if (controlBlock == NULL)
    {
        return;
    }

    bool queued = false;
    size_t length = 0;
    const uint8_t *span;
    // unsent() returns at most up to the ring's wrap, so wrapped data takes two
    // rounds
    while ((span = transmitRing.unsent(length)) != NULL)
    {
        size_t space = tcp_sndbuf(controlBlock);
        if (space == 0)
        {
            break;
        }
        if (length > space)
        {
            length = space;
        }
        if (length > 0xFFFF)
        {
            length = 0xFFFF;
        }

        u8_t flags = WRITE_FLAGS;
        if (length < unsentLength())
        {
            // More follows straight away; no PSH on this part
            flags |= TCP_WRITE_FLAG_MORE;
        }

        // Without TCP_WRITE_FLAG_COPY the bytes must stay unchanged until
        // acknowledged, which the ring guarantees: it frees them in onSent()
        err_t result = tcp_write(controlBlock, span, (u16_t)length, flags);
        if (result != ERR_OK)
        {
            // ERR_MEM: out of segments or pbufs for now
            if (result != ERR_MEM)
            {
                PICO_TCP_LOG("tcp_write failed: %d\n", result);
            }
            break;
        }
        transmitRing.markSent(length);
#if !PICO_TCP_ZERO_COPY_TRANSMIT
        // lwIP has its own copy
        transmitRing.release(length);
#endif
        queued = true;
    }

    if (queued)
    {
        tcp_output(controlBlock);
    }
}

void PicoTcpClient::applyNoDelay(struct tcp_pcb *controlBlock)
{
    if (controlBlock == NULL)
    {
        return;
    }
    if (noDelay)
    {
        tcp_nagle_disable(controlBlock);
    }
    else
    {
        tcp_nagle_enable(controlBlock);
    }
}

void PicoTcpClient::setNoDelay(bool value)
{
    cyw43_arch_lwip_begin();
    noDelay = value;
    applyNoDelay(activeBlock);
    cyw43_arch_lwip_end();
}

bool PicoTcpClient::getNoDelay()
{
    return noDelay;
}

/**
 * Discards unread data. With a connection it is also acknowledged, which
 * tcp_close() needs: closing with unacknowledged received data sends a RST and
 * frees the connection instead of sending FIN.
 */
void PicoTcpClient::dropReceived(struct tcp_pcb *controlBlock)
{
    if (receiveChain != NULL)
    {
        if (controlBlock != NULL)
        {
            tcp_recved(controlBlock, receiveChain->tot_len);
        }
        pbuf_free(receiveChain);
        receiveChain = NULL;
    }
    // Data refused at PICO_TCP_RECEIVE_PBUF_LIMIT, which lwIP is holding
    if (controlBlock != NULL && controlBlock->refused_data != NULL)
    {
        tcp_recved(controlBlock, controlBlock->refused_data->tot_len);
        pbuf_free(controlBlock->refused_data);
        controlBlock->refused_data = NULL;
    }
}

/**
 * Opens a connection to address and port.
 *
 * @return false If lwIP could not start it
 */
bool PicoTcpClient::startConnection()
{
    transmitRing.clear();
    dropReceived(NULL);
    isConnected = false;
    remoteClosed = false;

    struct tcp_pcb *controlBlock = tcp_new_ip_type(IP_GET_TYPE(&address));
    if (controlBlock == NULL)
    {
        return false;
    }

    activeBlock = controlBlock;
    tcp_arg(controlBlock, this);
    tcp_recv(controlBlock, lwipReceived);
    tcp_sent(controlBlock, lwipSent);
    tcp_poll(controlBlock, lwipPoll, POLL_INTERVAL);
    tcp_err(controlBlock, lwipError);
    applyNoDelay(controlBlock);

    err_t result = tcp_connect(controlBlock, &address, port, lwipConnected);
    if (result != ERR_OK)
    {
        PICO_TCP_LOG("tcp_connect failed: %d\n", result);
        activeBlock = NULL;
        detach(controlBlock);
        tcp_abort(controlBlock);
        return false;
    }
    return true;
}

void PicoTcpClient::onDnsFound(const char *hostname, const ip_addr_t *found)
{
    if (!resolving || hashName(hostname) != resolvingHash)
    {
        // stop() was called, or this answers an older request
        return;
    }
    resolving = false;

    if (found == NULL)
    {
        // connected() stays false, so MqttClient's connect timeout reports it
        PICO_TCP_LOG("Could not resolve %s\n", hostname);
        return;
    }
    address = *found;
    startConnection();
}

/**
 * Moves the active connection to closing. It stays open until every committed
 * byte has been handed to lwIP, then queues FIN, and is let go once the broker
 * has acknowledged everything sent from the ring.
 */
bool PicoTcpClient::beginClose()
{
    struct tcp_pcb *controlBlock = activeBlock;
    if (controlBlock == NULL)
    {
        return false;
    }
    activeBlock = NULL;
    isConnected = false;
    remoteClosed = false;

    dropReceived(controlBlock);

    if (controlBlock->state != ESTABLISHED && controlBlock->state != CLOSE_WAIT)
    {
        // Still connecting: nothing has been sent. tcp_close() would free a
        // connecting pcb at once, so abort it explicitly.
        detach(controlBlock);
        tcp_abort(controlBlock);
        transmitRing.clear();
        return true;
    }

    if (closingBlock != NULL)
    {
        // connect() does not start a connection while one is closing, so this
        // only guards the ring against two users
        abortClosing();
    }

    closingBlock = controlBlock;
    finQueued = false;
    closePolls = 0;
    tcp_err(controlBlock, lwipClosingError);
    // Deliver the remaining data without waiting on Nagle
    tcp_nagle_disable(controlBlock);
    return advanceClose();
}

bool PicoTcpClient::advanceClose()
{
    struct tcp_pcb *controlBlock = closingBlock;
    if (controlBlock == NULL)
    {
        return false;
    }

    if (!finQueued)
    {
        sendPending(controlBlock);
        if (unsentLength() > 0)
        {
            // FIN goes after the last byte; wait for room in the send buffer
            return false;
        }
        if (tcp_close(controlBlock) != ERR_OK)
        {
            // No memory to queue FIN
            abortClosing();
            return true;
        }
        finQueued = true;
    }

    if (transmitRing.unacknowledged() == 0)
    {
        // Nothing of ours is left for lwIP to resend; it finishes the close
        // on its own
        detach(controlBlock);
        closingBlock = NULL;
        transmitRing.clear();
    }
    return false;
}

void PicoTcpClient::abortClosing()
{
    struct tcp_pcb *controlBlock = closingBlock;
    if (controlBlock == NULL)
    {
        return;
    }
    closingBlock = NULL;
    detach(controlBlock);
    tcp_abort(controlBlock);
    transmitRing.clear();
}

err_t PicoTcpClient::onReceived(struct tcp_pcb *controlBlock, struct pbuf *payload, err_t errorCode)
{
    if (controlBlock != activeBlock)
    {
        // A closing connection: nothing reads its data any more
        if (payload != NULL)
        {
            tcp_recved(controlBlock, payload->tot_len);
            pbuf_free(payload);
        }
        return ERR_OK;
    }

    if (payload == NULL)
    {
        PICO_TCP_LOG("Remote closed the connection\n");
        if (receiveChain != NULL)
        {
            // Let the application read what arrived first; read() closes
            remoteClosed = true;
            return ERR_OK;
        }
        return beginClose() ? ERR_ABRT : ERR_OK;
    }

    if (errorCode != ERR_OK)
    {
        tcp_recved(controlBlock, payload->tot_len);
        pbuf_free(payload);
        return ERR_OK;
    }

    if (receiveChain != NULL &&
        pbuf_clen(receiveChain) + pbuf_clen(payload) > PICO_TCP_RECEIVE_PBUF_LIMIT)
    {
        // lwIP keeps the pbuf and offers it again on its timer
        return ERR_MEM;
    }

    // Keep lwIP's buffers; read() copies straight out of them. They are
    // acknowledged only once read, so the chain is bounded by the TCP window.
    if (receiveChain == NULL)
    {
        receiveChain = payload;
    }
    else
    {
        pbuf_cat(receiveChain, payload);
    }
    return ERR_OK;
}

err_t PicoTcpClient::onSent(struct tcp_pcb *controlBlock, u16_t length)
{
    if (controlBlock != activeBlock && controlBlock != closingBlock)
    {
        return ERR_OK;
    }

#if PICO_TCP_ZERO_COPY_TRANSMIT
    // lwIP has dropped its references to the acknowledged bytes
    transmitRing.release(length);
#else
    // Already released when lwIP copied them
    (void)length;
#endif

    if (controlBlock == activeBlock)
    {
        sendPending(controlBlock);
        return ERR_OK;
    }
    return advanceClose() ? ERR_ABRT : ERR_OK;
}

err_t PicoTcpClient::onPoll(struct tcp_pcb *controlBlock)
{
    if (controlBlock == activeBlock)
    {
        sendPending(controlBlock);
    }
    else if (controlBlock == closingBlock)
    {
        if (++closePolls >= CLOSE_POLL_LIMIT)
        {
            PICO_TCP_LOG("Close timed out; resetting\n");
            abortClosing();
            return ERR_ABRT;
        }
        return advanceClose() ? ERR_ABRT : ERR_OK;
    }
    return ERR_OK;
}

void PicoTcpClient::onError()
{
    activeBlock = NULL;
    isConnected = false;
    remoteClosed = false;
    transmitRing.clear();
    // The connection is gone, so there is nothing to acknowledge to
    dropReceived(NULL);
}

void PicoTcpClient::onClosingError()
{
    closingBlock = NULL;
    transmitRing.clear();
}

int PicoTcpClient::connect(const char *host, uint16_t port)
{
    if (host == NULL || *host == '\0')
    {
        return BASE_ERROR;
    }

    cyw43_arch_lwip_begin();

    if (activeBlock != NULL || resolving)
    {
        // Already connected or connecting
        cyw43_arch_lwip_end();
        return BASE_ERROR;
    }

    if (closingBlock != NULL)
    {
        // The previous connection may still resend from the ring
        PICO_TCP_LOG("Previous connection still closing\n");
        cyw43_arch_lwip_end();
        return BASE_ERROR;
    }

    this->port = port;
    bool started = false;

    if (ipaddr_aton(host, &address))
    {
        started = startConnection();
    }
    else
    {
#if LWIP_DNS
        err_t result = dns_gethostbyname(host, &address, lwipDnsFound, this);
        if (result == ERR_OK)
        {
            // Already cached
            started = startConnection();
        }
        else if (result == ERR_INPROGRESS)
        {
            // onDnsFound() connects
            resolving = true;
            resolvingHash = hashName(host);
            started = true;
        }
        else
        {
            PICO_TCP_LOG("DNS lookup failed to start: %d\n", result);
        }
#else
        PICO_TCP_LOG("Not an IP address: %s\n", host);
#endif
    }

    cyw43_arch_lwip_end();

    return started ? 1 : BASE_ERROR;
}

size_t PicoTcpClient::write(uint8_t data)
{
    return write(&data, 1);
}

size_t PicoTcpClient::write(const void *buffer, size_t length)
{
    if (buffer == NULL || length == 0 || length > writable())
    {
        return 0;
    }

    const uint8_t *source = (const uint8_t *)buffer;
    size_t done = 0;
    while (done < length)
    {
        size_t contiguous = 0;
        uint8_t *destination = writePointer(done, contiguous);
        if (destination == NULL)
        {
            return 0;
        }
        size_t count = (length - done) < contiguous ? (length - done) : contiguous;
        memcpy(destination, source + done, count);
        done += count;
    }
    return commit(length) ? length : 0;
}

size_t PicoTcpClient::writable()
{
    cyw43_arch_lwip_begin();
    // Nothing can be written without a connection to send it on
    size_t length = (activeBlock != NULL && isConnected) ? transmitRing.writable() : 0;
    cyw43_arch_lwip_end();
    return length;
}

uint8_t *PicoTcpClient::writePointer(size_t offset, size_t &contiguous)
{
    // Free space past the committed data, which lwIP never touches, so this
    // needs no lock. writable() only grows while it is being written.
    return transmitRing.writePointer(offset, contiguous);
}

bool PicoTcpClient::commit(size_t length)
{
    cyw43_arch_lwip_begin();
    bool accepted = activeBlock != NULL && isConnected && transmitRing.commit(length);
    if (accepted)
    {
        sendPending(activeBlock);
    }
    cyw43_arch_lwip_end();
    return accepted;
}

int PicoTcpClient::available()
{
    cyw43_arch_lwip_begin();
    int length = (receiveChain != NULL) ? receiveChain->tot_len : 0;
    cyw43_arch_lwip_end();
    return length;
}

int PicoTcpClient::read(void *buffer, size_t length)
{
    if (buffer == NULL || length == 0)
    {
        return 0;
    }

    cyw43_arch_lwip_begin();
    if (receiveChain == NULL)
    {
        cyw43_arch_lwip_end();
        return 0;
    }

    u16_t count = (length < receiveChain->tot_len) ? (u16_t)length : receiveChain->tot_len;
    pbuf_copy_partial(receiveChain, buffer, count, 0);
    // Frees the pbufs read completely and moves past the rest
    receiveChain = pbuf_free_header(receiveChain, count);

    if (activeBlock != NULL)
    {
        // Open the window again now that the data has been consumed
        tcp_recved(activeBlock, count);
        if (receiveChain == NULL && remoteClosed)
        {
            // The broker closed and everything it sent has been read
            beginClose();
        }
    }
    cyw43_arch_lwip_end();
    return count;
}

void PicoTcpClient::stop()
{
    cyw43_arch_lwip_begin();
    // A lookup in progress cannot be cancelled; its answer is ignored
    resolving = false;
    beginClose();
    dropReceived(NULL);
    cyw43_arch_lwip_end();
}

uint8_t PicoTcpClient::connected()
{
    return isConnected;
}

void PicoTcpClient::sync()
{
#if PICO_CYW43_ARCH_POLL
    cyw43_arch_poll();
#endif
}
