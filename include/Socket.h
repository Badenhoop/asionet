//
// Created by philipp on 02.01.18.
//

#ifndef NETWORKINGLIB_SOCKETOPS_H
#define NETWORKINGLIB_SOCKETOPS_H

#include "Stream.h"
#include "Resolver.h"
#include "Frame.h"

namespace asionet
{
namespace socket
{

namespace internal
{

inline bool stringFromBuffer(std::string & data, std::vector<char> & buffer, std::size_t numBytesTransferred)
{
    if (numBytesTransferred < 4)
        return false;

    auto numDataBytes = utils::fromBigEndian<4, std::uint32_t>((const std::uint8_t *) buffer.data());
    if (numBytesTransferred < 4 + numDataBytes)
        return false;

    data = std::string{buffer.begin() + 4,
                       buffer.begin() + 4 + numDataBytes};
    return true;
}

}

using ConnectHandler = std::function<void(const error::ErrorCode & error)>;

using SendHandler = std::function<void(const error::ErrorCode & error)>;

using ReceiveHandler = std::function<void(const error::ErrorCode & error,
                                          std::string & data,
                                          const std::string & host,
                                          std::uint16_t port)>;

template<typename SocketService>
void asyncConnect(asionet::Context & context,
                  SocketService & socket,
                  const std::string & host,
                  std::uint16_t port,
                  const time::Duration & timeout,
                  const ConnectHandler & handler)
{
    using namespace asionet::internal;
    using Resolver = CloseableResolver<boost::asio::ip::tcp>;

    auto startTime = time::now();

    // Resolve host.
    auto resolver = std::make_shared<Resolver>(context);
    Resolver::Query query{host, std::to_string(port)};

    auto resolveOperation = [&resolver](auto && ... args)
    { resolver->async_resolve(std::forward<decltype(args)>(args)...); };

    closeable::timedAsyncOperation(
        context, resolveOperation, *resolver, timeout,
        [&context, &socket, host, port, timeout, handler, resolver, startTime]
            (const auto & networkingError, const auto & boostError, auto endpointIterator)
        {
            if (networkingError)
            {
                handler(networkingError);
                return;
            }

            // Update timeout.
            auto timeSpend = time::now() - startTime;
            auto newTimeout = timeout - timeSpend;

            auto connectOperation = [](auto && ... args)
            { boost::asio::async_connect(std::forward<decltype(args)>(args)...); };

            closeable::timedAsyncOperation(
                context, connectOperation, socket, newTimeout,
                [handler](const auto & networkingError, const auto & boostError, auto iterator)
                {
                    handler(networkingError);
                },
                socket, endpointIterator);
        },
        query);
}

template<typename DatagramSocket>
void asyncSendTo(asionet::Context & context,
                 DatagramSocket & socket,
                 std::shared_ptr<std::string> sendData,
                 const std::string & host,
                 std::uint16_t port,
                 const time::Duration & timeout,
                 const SendHandler & handler)
{
    using namespace boost::asio::ip;
    udp::endpoint endpoint{address::from_string(host), port};

    using namespace asionet::internal;
    auto buffer = std::make_shared<Frame>((const std::uint8_t *) sendData->c_str(), sendData->size());

    auto asyncOperation = [&socket](auto && ... args)
    { socket.async_send_to(std::forward<decltype(args)>(args)...); };

    closeable::timedAsyncOperation(
        context, asyncOperation, socket, timeout,
        [handler, buffer, sendData](const auto & networkingError,
                          const auto & boostError,
                          auto numBytesTransferred)
        {
            if (numBytesTransferred < buffer->getSize())
            {
                handler(error::codes::FAILED_OPERATION);
                return;
            }

            handler(networkingError);
        },
        buffer->getBuffers(), endpoint);
};

template<typename DatagramSocket>
void asyncReceiveFrom(asionet::Context & context,
                      DatagramSocket & socket,
                      std::vector<char> & buffer,
                      const time::Duration & timeout,
                      const ReceiveHandler & handler)
{
    using namespace boost::asio::ip;
    auto senderEndpoint = std::make_shared<udp::endpoint>();

    auto asyncOperation = [&socket](auto && ... args)
    { socket.async_receive_from(std::forward<decltype(args)>(args)...); };

    closeable::timedAsyncOperation(
        context, asyncOperation, socket, timeout,
        [&buffer, handler, senderEndpoint](const auto & networkingError, const auto & boostError, auto numBytesTransferred)
        {
            std::string receiveData{};
            auto senderHost = senderEndpoint->address().to_string();
            auto senderPort = senderEndpoint->port();

            if (networkingError)
            {
                handler(networkingError, receiveData, senderHost, senderPort);
                return;
            }

            if (!internal::stringFromBuffer(receiveData, buffer, numBytesTransferred))
            {
                handler(error::codes::FAILED_OPERATION, receiveData, senderHost, senderPort);
                return;
            }

            handler(networkingError, receiveData, senderHost, senderPort);
        },
        boost::asio::buffer(buffer),
        *senderEndpoint);
}

}
}

#endif //NETWORKINGLIB_SOCKETOPS_H
