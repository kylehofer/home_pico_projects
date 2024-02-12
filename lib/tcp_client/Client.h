/*
 * File: Client.h
 * Project: pico_tcp_client
 * Created Date: Thursday December 29th 2022
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

#ifndef CLIENT
#define CLIENT

#include <stdlib.h>
#include <stdint.h>

/**
 * @brief Interface for representing a communication client
 */
class Client
{
public:
    virtual int connect(const char *host, uint16_t port) = 0;
    virtual size_t write(uint8_t) = 0;
    virtual size_t write(const void *buffer, size_t size) = 0;
    virtual int available() = 0;
    virtual int read(void *buffer, size_t size) = 0;
    virtual void stop() = 0;
    virtual uint8_t connected() = 0;
    virtual void sync() = 0;
};

#endif /* CLIENT */