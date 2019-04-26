/*
 * The MIT License
 * 
 * Copyright (c) 2019 Philipp Badenhoop
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#ifndef ASIONET_SOCKET_H
#define ASIONET_SOCKET_H

#include "Stream.h"
#include "Resolver.h"
#include "Frame.h"
#include "ConstBuffer.h"

namespace asionet
{
namespace socket
{

namespace internal
{

inline bool numDataBytesFromBuffer(const std::vector<char> & buffer, std::size_t numBytesTransferred, std::size_t & numDataBytes)
{
    if (numBytesTransferred < 4)
        return false;

    numDataBytes = utils::fromBigEndian<4, std::uint32_t>((const std::uint8_t *) buffer.data());
    if (numBytesTransferred < 4 + numDataBytes)
        return false;

    return true;
}

}

using ConnectHandler = std::function<void(const error::Error & error)>;

using SendHandler = std::function<void(const error::Error & error)>;

using ReceiveHandler = std::function<void(const error::Error & error,
                                          const asionet::internal::ConstVectorBuffer & buffer,
                                          const boost::asio::ip::udp::endpoint & endpoint)>;

template<typename SocketService>
void asyncConnect(SocketService & socket,
                  const std::string & host,
                  std::uint16_t port,
                  const time::Duration & timeout,
                  ConnectHandler handler)
{
    auto & context = socket.get_executor().context();
    using namespace asionet::internal;
    using Resolver = CloseableResolver<boost::asio::ip::tcp>;

    auto startTime = time::now();

    // Resolve host.
    auto resolver = std::make_shared<Resolver>(context);
    Resolver::Query query{host, std::to_string(port)};

    auto resolveOperation = [&resolver](auto && ... args)
    { resolver->async_resolve(std::forward<decltype(args)>(args)...); };

    closeable::timedAsyncOperation(
        resolveOperation, *resolver, timeout,
        [&socket, host, port, timeout, handler = std::move(handler), resolver, startTime]
            (const auto & error, const auto & endpointIterator)
        {
            if (error)
            {
                handler(error);
                return;
            }

            // Update timeout.
            auto timeSpend = time::now() - startTime;
            auto newTimeout = timeout - timeSpend;

            auto connectOperation = [](auto && ... args)
            { boost::asio::async_connect(std::forward<decltype(args)>(args)...); };

            closeable::timedAsyncOperation(
                connectOperation, socket, newTimeout,
                [handler](const auto & error, auto iterator)
                {
                    handler(error);
                },
                socket, endpointIterator);
        },
        query);
}

template<typename SocketService, typename EndpointIterator>
void asyncConnect(SocketService & socket,
                  const EndpointIterator & endpointIterator,
                  const time::Duration & timeout,
                  ConnectHandler handler)
{
    auto connectOperation = [](auto && ... args)
    { boost::asio::async_connect(std::forward<decltype(args)>(args)...); };

    closeable::timedAsyncOperation(
        connectOperation, socket, timeout,
        [handler = std::move(handler)](const auto & error, auto iterator)
        {
            handler(error);
        },
        socket, endpointIterator);
}

template<typename DatagramSocket>
void asyncSendTo(DatagramSocket & socket,
                 const std::string & sendData,
                 const std::string & ip,
                 std::uint16_t port,
                 const time::Duration & timeout,
                 SendHandler handler)
{
    using Endpoint = boost::asio::ip::udp::endpoint;
    asyncSendTo(socket, sendData, Endpoint{boost::asio::ip::address::from_string(ip), port}, timeout, handler);
};

template<typename DatagramSocket, typename Endpoint>
void asyncSendTo(DatagramSocket & socket,
                 const std::string & sendData,
                 const Endpoint & endpoint,
                 const time::Duration & timeout,
                 SendHandler handler)
{
    using namespace asionet::internal;
    auto frame = std::make_shared<Frame>((const std::uint8_t *) sendData.c_str(), sendData.size());
    auto && buffers = frame->getBuffers();

    auto asyncOperation = [&socket](auto && ... args)
    { socket.async_send_to(std::forward<decltype(args)>(args)...); };

    closeable::timedAsyncOperation(
        asyncOperation, socket, timeout,
        [handler = std::move(handler), frame = std::move(frame)](const auto & error, auto numBytesTransferred)
        {
            if (numBytesTransferred < frame->getSize())
            {
                handler(error::failedOperation);
                return;
            }

            handler(error);
        },
        buffers, endpoint);
};

template<typename DatagramSocket>
void asyncReceiveFrom(DatagramSocket & socket,
                      std::vector<char> & buffer,
                      const time::Duration & timeout,
                      ReceiveHandler handler)
{
    using asionet::internal::Frame;
    using asionet::internal::ConstVectorBuffer;
    using namespace boost::asio::ip;
    auto senderEndpoint = std::make_shared<udp::endpoint>();

    // because of std::move()
    auto & senderEndpointRef = *senderEndpoint;

    auto asyncOperation = [&socket](auto && ... args)
    { socket.async_receive_from(std::forward<decltype(args)>(args)...); };

    closeable::timedAsyncOperation(
        asyncOperation, socket, timeout,
        [&buffer, handler = std::move(handler), senderEndpoint = std::move(senderEndpoint)](const auto & error, auto numBytesTransferred)
        {
            if (error)
            {
                handler(error, ConstVectorBuffer{buffer, 0, 0}, *senderEndpoint);
                return;
            }

            std::size_t numDataBytes{0};
            if (!internal::numDataBytesFromBuffer(buffer, numBytesTransferred, numDataBytes))
            {
                handler(error::invalidFrame, ConstVectorBuffer{buffer, 0, 0}, *senderEndpoint);
                return;
            }

            handler(error, ConstVectorBuffer{buffer, numDataBytes, Frame::HEADER_SIZE}, *senderEndpoint);
        },
        boost::asio::buffer(buffer),
        senderEndpointRef);
}

}
}

#endif //ASIONET_SOCKET_H
