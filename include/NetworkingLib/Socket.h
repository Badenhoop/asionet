//
// Created by philipp on 02.01.18.
//

#ifndef NETWORKINGLIB_SOCKETOPS_H
#define NETWORKINGLIB_SOCKETOPS_H

#include "Stream.h"
#include "Resolver.h"
#include "Frame.h"

namespace networking
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
void connect(Networking & net,
             SocketService & socket,
             const std::string & host,
             std::uint16_t port,
             time::Duration timeout)
{
    using namespace networking::internal;
    using Resolver = CloseableResolver<boost::asio::ip::tcp>;

    auto startTime = time::now();

    // Resolve host.
    Resolver resolver{net.getIoService()};
    Resolver::Query query{host, std::to_string(port)};

    auto resolveOperation = [&resolver](auto && ... args)
    { resolver.async_resolve(std::forward<decltype(args)>(args)...); };

    std::tuple<boost::system::error_code, Resolver::Iterator> resolveResult;
    closeable::timedOperation(
        resolveResult, net, resolveOperation, resolver, timeout, query);

    auto endpointIterator = std::get<1>(resolveResult);

    // Update timeout.
    auto timeSpend = time::now() - startTime;
    timeout -= timeSpend;

    auto connectOperation = [](auto && ... args)
    { boost::asio::async_connect(std::forward<decltype(args)>(args)...); };

    std::tuple<boost::system::error_code, Resolver::Iterator> connectResult;
    closeable::timedOperation(
        connectResult, net, connectOperation, socket, timeout, socket, endpointIterator);
}

template<typename SocketService>
void asyncConnect(Networking & net,
                  SocketService & socket,
                  const std::string & host,
                  std::uint16_t port,
                  const time::Duration & timeout,
                  const ConnectHandler & handler)
{
    using namespace networking::internal;
    using Resolver = CloseableResolver<boost::asio::ip::tcp>;

    auto startTime = time::now();

    // Resolve host.
    auto resolver = std::make_shared<Resolver>(net.getIoService());
    Resolver::Query query{host, std::to_string(port)};

    auto resolveOperation = [&resolver](auto && ... args)
    { resolver->async_resolve(std::forward<decltype(args)>(args)...); };

    closeable::timedAsyncOperation(
        net, resolveOperation, *resolver, timeout,
        [&net, &socket, host, port, timeout, handler, resolver, startTime]
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
                net, connectOperation, socket, newTimeout,
                [handler](const auto & networkingError, const auto & boostError, auto iterator)
                {
                    handler(networkingError);
                },
                socket, endpointIterator);
        },
        query);
}

template<typename DatagramSocket>
void sendTo(Networking & net,
            DatagramSocket & socket,
            const std::string & sendData,
            const std::string & host,
            std::uint16_t port,
            const time::Duration & timeout)
{
    using namespace boost::asio::ip;
    udp::endpoint endpoint{address::from_string(host), port};

    using namespace networking::internal;
    Frame buffer{(const std::uint8_t *) sendData.c_str(), (std::uint32_t) sendData.size()};

    auto asyncOperation = [&socket](auto && ... args)
    { socket.async_send_to(std::forward<decltype(args)>(args)...); };

    std::tuple<boost::system::error_code, std::size_t> result;
    closeable::timedOperation(
        result, net, asyncOperation, socket, timeout, buffer.getBuffers(), endpoint);

    auto numBytesTransferred = std::get<1>(result);
    if (numBytesTransferred < buffer.getSize())
        throw error::FailedOperation{};
};

template<typename DatagramSocket>
void asyncSendTo(Networking & net,
                 DatagramSocket & socket,
                 const std::string & sendData,
                 const std::string & host,
                 std::uint16_t port,
                 const time::Duration & timeout,
                 const SendHandler & handler)
{
    using namespace boost::asio::ip;
    udp::endpoint endpoint{address::from_string(host), port};

    using namespace networking::internal;
    auto buffer = std::make_shared<Frame>((const std::uint8_t *) sendData.c_str(), sendData.size());

    auto asyncOperation = [&socket](auto && ... args)
    { socket.async_send_to(std::forward<decltype(args)>(args)...); };

    closeable::timedAsyncOperation(
        net, asyncOperation, socket, timeout,
        [handler, buffer](const auto & networkingError,
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
std::string receiveFrom(Networking & net,
                        DatagramSocket & socket,
                        std::vector<char> buffer,
                        std::string & senderHost,
                        std::uint16_t & senderPort,
                        const time::Duration & timeout)
{
    using namespace boost::asio::ip;
    udp::endpoint senderEndpoint;

    auto asyncOperation = [&socket](auto && ... args)
    { socket.async_receive_from(std::forward<decltype(args)>(args)...); };

    std::tuple<boost::system::error_code, std::size_t> result;
    closeable::timedOperation(
        result, net, asyncOperation, socket, timeout, boost::asio::buffer(buffer), senderEndpoint);

    senderHost = senderEndpoint.address().to_string();
    senderPort = senderEndpoint.port();

    std::string receiveData{};
    auto numBytesTransferred = std::get<1>(result);
    if (!internal::stringFromBuffer(receiveData, buffer, numBytesTransferred))
        throw error::FailedOperation{};
    return receiveData;
}

template<typename DatagramSocket>
void asyncReceiveFrom(Networking & net,
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
        net, asyncOperation, socket, timeout,
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
