//
// Created by philipp on 01.12.17.
//

#ifndef PROTOCOL_TCPNETWORKSERVICECLIENT_H
#define PROTOCOL_TCPNETWORKSERVICECLIENT_H

#include <string>
#include <functional>
#include <boost/asio/io_service.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/connect.hpp>
#include "Message.h"
#include "Networking.h"
#include "Utils.h"
#include "Error.h"
#include "Busyable.h"

namespace asionet
{
namespace service
{

template<typename Service>
class Client
    : public std::enable_shared_from_this<Client<Service>>
      , public Busyable
{
private:
    struct PrivateTag
    {
    };

public:
    using Ptr = std::shared_ptr<Client<Service>>;

    using RequestMessage = typename Service::RequestMessage;
    using ResponseMessage = typename Service::ResponseMessage;

    using CallHandler = std::function<void(const error::ErrorCode & error, ResponseMessage & response)>;

    static Ptr create(Networking & net, std::size_t maxMessageSize = 512)
    {
        return std::make_shared<Client<Service>>(PrivateTag{}, net, maxMessageSize);
    }

    Client(PrivateTag, Networking & net, std::size_t maxMessageSize)
        : net(net)
          , socket(net.getIoService())
          , maxMessageSize(maxMessageSize)
    {}

    void call(const RequestMessage & request,
              ResponseMessage & response,
              const std::string & host,
              std::uint16_t port,
              time::Duration timeout)
    {
        BusyLock busyLock{*this};
        // Close the socket on leaving.
        closeable::Closer<Socket> socketCloser{socket};

        // We have three places to lose time: connecting, sending and receiving.
        // So after each operation we subtract the time spend from our timeout.
        auto startTime = time::now();
        newSocket();
        // Connect to server.
        asionet::socket::connect(net, socket, host, port, timeout);
        updateTimeout(timeout, startTime);
        // Send the request.
        message::send(net, socket, request, timeout);
        updateTimeout(timeout, startTime);
        // Receive the response.
        boost::asio::streambuf buffer{maxMessageSize + internal::Frame::HEADER_SIZE};
        message::receive<ResponseMessage>(net, socket, buffer, response, timeout);
    }

    void asyncCall(const RequestMessage & request,
                   const std::string & host,
                   std::uint16_t port,
                   const time::Duration & timeout,
                   const CallHandler & handler)
    {
        auto self = this->shared_from_this();
        // Container for our variables which are needed for the subsequent asynchronous calls to connect, receive and send.
        // When 'state' goes out of scope, it does cleanup.
        auto state = std::make_shared<AsyncState>(
            self, handler, timeout, time::now());

        newSocket();

        // Connect to server.
        asionet::socket::asyncConnect(
            net, socket, host, port, state->timeout,
            [state, request](const auto & error)
            {
                if (error)
                {
                    ResponseMessage noResponse;
                    state->handler(error, noResponse);
                    return;
                }

                Client<Service>::updateTimeout(state->timeout, state->startTime);

                // Send the request.
                asionet::message::asyncSend(
                    state->self->net, state->self->socket, request, state->timeout,
                    [state](const auto & error)
                    {
                        if (error)
                        {
                            ResponseMessage noResponse;
                            state->handler(error, noResponse);
                            return;
                        }

                        Client<Service>::updateTimeout(state->timeout, state->startTime);

                        // Receive the response.
                        asionet::message::asyncReceive<ResponseMessage>(
                            state->self->net, state->self->socket, state->buffer, state->timeout,
                            [state](auto const & error, auto & response)
                            {
                                state->handler(error, response);
                            });
                    });
            });
    }

    bool isCalling() const noexcept
    {
        return isBusy();
    }

    void stop()
    {
        closeable::Closer<Socket>::close(socket);
    }

private:
    using Socket = boost::asio::ip::tcp::socket;
    using Frame = asionet::internal::Frame;

    // We must keep track of some variables during the async handler chain.
    struct AsyncState
    {
        AsyncState(Ptr self,
                   const CallHandler & handler,
                   time::Duration timeout,
                   time::TimePoint startTime)
            : busyLock(*self)
              , self(self)
              , handler(handler)
              , timeout(timeout)
              , startTime(startTime)
              , buffer(self->maxMessageSize + Frame::HEADER_SIZE)
              , closer(self->socket)
        {}

        BusyLock busyLock;
        Ptr self;
        CallHandler handler;
        time::Duration timeout;
        time::TimePoint startTime;
        boost::asio::streambuf buffer;
        closeable::Closer<Socket> closer;
    };

    asionet::Networking & net;
    Socket socket;
    std::size_t maxMessageSize;

    static void updateTimeout(time::Duration & timeout, time::TimePoint & startTime)
    {
        auto nowTime = time::now();
        auto timeSpend = nowTime - startTime;
        startTime = nowTime;
        timeout -= timeSpend;
    }

    void newSocket()
    {
        if (!socket.is_open())
            socket = Socket(net.getIoService());
    }
};

}
}


#endif //PROTOCOL_TCPNETWORKSERVICECLIENT_H
