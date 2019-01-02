//
// Created by philipp on 15.01.18.
//

#ifndef NETWORKINGLIB_DATAGRAMRECEIVER_H
#define NETWORKINGLIB_DATAGRAMRECEIVER_H

#include "Stream.h"
#include "Socket.h"
#include "Message.h"
#include "Context.h"

namespace asionet
{

template<typename Message>
class DatagramReceiver
    : public std::enable_shared_from_this<DatagramReceiver<Message>>
      , public Busyable
{
private:
    struct PrivateTag
    {
    };

public:
    using Ptr = std::shared_ptr<DatagramReceiver<Message>>;

    using ReceiveHandler = std::function<
        void(const error::ErrorCode & error,
             const std::shared_ptr<Message> & message,
             const std::string & host,
             std::uint16_t port)>;

    static Ptr create(asionet::Context & context, std::uint16_t bindingPort, std::size_t maxMessageSize = 512)
    {
        return std::make_shared<DatagramReceiver<Message>>(PrivateTag{}, context, bindingPort, maxMessageSize);
    }

    DatagramReceiver(PrivateTag, asionet::Context & context, std::uint16_t bindingPort, std::size_t maxMessageSize)
        : context(context)
          , bindingPort(bindingPort)
          , socket(context)
          , buffer(maxMessageSize + Frame::HEADER_SIZE)
    {}

    std::shared_ptr<Message> receive(std::string & host, std::uint16_t & port, const time::Duration & timeout)
    {
        BusyLock busyLock{*this};
        setupSocket();
        return message::receiveDatagram<Message>(context, socket, buffer, host, port, timeout);
    }

    void asyncReceive(const time::Duration & timeout, const ReceiveHandler & handler)
    {
        auto self = this->shared_from_this();
        auto state = std::make_shared<AsyncState>(self, handler);

        setupSocket();

        message::asyncReceiveDatagram<Message>(
            context, socket, buffer, timeout,
            [state](const auto & error,
                    const auto & message,
                    const auto & senderHost,
                    auto senderPort)
            {
                state->busyLock.unlock();
                state->handler(error, message, senderHost, senderPort);
            });
    }

    bool isReceiving() const noexcept
    {
        return isBusy();
    };

    void stop()
    {
        closeable::Closer<Socket>::close(socket);
    }

private:
    using Udp = boost::asio::ip::udp;
    using Endpoint = Udp::endpoint;
    using Socket = Udp::socket;
    using Frame = asionet::internal::Frame;

    asionet::Context & context;
    std::uint16_t bindingPort;
    Socket socket;
    std::vector<char> buffer;

    void setupSocket()
    {
        if (socket.is_open())
            return;

        socket.open(Udp::v4());
        socket.set_option(boost::asio::socket_base::reuse_address{true});
        socket.set_option(boost::asio::socket_base::broadcast{true});
        socket.bind(Endpoint(Udp::v4(), bindingPort));
    }

    struct AsyncState
    {
        AsyncState(Ptr self, const ReceiveHandler & handler)
            : busyLock(*self)
              , self(self)
              , handler(handler)
        {}

        BusyLock busyLock;
        Ptr self;
        ReceiveHandler handler;
    };
};

}

#endif //NETWORKINGLIB_DATAGRAMRECEIVER_H
