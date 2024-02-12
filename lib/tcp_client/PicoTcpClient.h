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

#include "stdint.h"
#include "Client.h"

#define BASE_ERROR -1
#define BUFFER_SIZE 2048

typedef struct DataBuffer_t
{
    const uint8_t *data;
    const uint8_t *position;
    size_t length;
    DataBuffer_t *next;
} DataBuffer;

#define DataBuffer_initializer \
    {                          \
        NULL, 0, NULL          \
    }

class PicoTcpClient : Client
{
private:
    struct tcp_pcb *tcpControlBlock = NULL;
    int availableData = 0;
    uint8_t *receivedData = NULL;

    DataBuffer *writeQueue = NULL;
    DataBuffer *receiveQueue = NULL;

    bool isConnected = false;
    bool isConfigured = false;
    bool isConnecting = false;
    bool waitingReply = false;

    struct Private;

    int8_t writeDataBuffer(DataBuffer *buffer);

    int8_t onConnected(int errorCode);
    int8_t received(void *data, int8_t errorCode);
    int sent(uint16_t length);
    int8_t poll();
    void onError(int8_t errorCode);

protected:
public:
    PicoTcpClient();
    ~PicoTcpClient();

    virtual int connect(const char *host, uint16_t port) override;
    virtual size_t write(uint8_t) override;
    virtual size_t write(const void *buffer, size_t length) override;
    virtual int available() override;
    virtual int read(void *buffer, size_t length) override;
    virtual void stop() override;
    virtual uint8_t connected() override;
    virtual void sync() override;
    void close();
};

#endif /* PICOTCPCLIENT */
