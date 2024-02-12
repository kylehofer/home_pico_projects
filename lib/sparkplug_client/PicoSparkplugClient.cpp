/*
 * File: PicoClient.h
 * Project: cpp_sparkplug
 * Created Date: Monday June 12th 2023
 * Author: Kyle Hofer
 *
 * MIT License
 *
 * Copyright (c) 2024 Kyle Hofer
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

#include "PicoSparkplugClient.h"

Client *PicoSparkplugClient::getClient()
{
    return (Client *)&tcpClient;
}

PicoSparkplugClient::PicoSparkplugClient(ClientEventHandler *handler, ClientOptions *options) : CppMqttClient(handler, options)
{
}

time_t PicoSparkplugClient::getTime()
{
    if (ntpClient)
    {
        return ntpClient->getTime();
    }

    return us_to_ms(time_us_64());
}

void PicoSparkplugClient::useNtpServer(std::string address, int port)
{
    ntpClient = NtpClient::create(address, port, 3600);
}

void PicoSparkplugClient::sync()
{
    if (ntpClient)
    {
        ntpClient->sync();
    }
    CppMqttClient::sync();
}

bool PicoSparkplugClient::isConnected()
{
    if (ntpClient)
    {
        return ntpClient->synced() && CppMqttClient::isConnected();
    }
    return CppMqttClient::isConnected();
}
