//
// Created by philipp on 15.01.18.
//

#ifndef NETWORKINGLIB_DATAGRAMRECEIVER_H
#define NETWORKINGLIB_DATAGRAMRECEIVER_H

#include "Networking.h"
#include "Stream.h"
#include "Socket.h"
#include "Message.h"

namespace networking
{
namespace message
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
             Message & message,
             const std::string & host,
             std::uint16_t port)>;

    static Ptr create(Networking & net, std::uint16_t bindingPort, std::size_t maxMessageSize = 512)
    {
        return std::make_shared<DatagramReceiver<Message>>(PrivateTag{}, net, bindingPort, maxMessageSize);
    }

    DatagramReceiver(PrivateTag, Networking & net, std::uint16_t bindingPort, std::size_t maxMessageSize)
        : net(net)
          , bindingPort(bindingPort)
          , socket(net.getIoService())
          , buffer(maxMessageSize + Frame::HEADER_SIZE)
    {}

    void receive(Message & message, std::string & host, std::uint16_t & port, const time::Duration & timeout)
    {
        BusyLock busyLock{*this};
        setupSocket();
        message::receiveDatagram<Message>(net, socket, buffer, message, host, port, timeout);
    }

    void asyncReceive(const time::Duration & timeout, const ReceiveHandler & handler)
    {
        auto self = this->shared_from_this();
        auto state = std::make_shared<AsyncState>(self, handler);

        setupSocket();

        message::asyncReceiveDatagram<Message>(
            net, socket, buffer, timeout,
            [state](const auto & error,
                    auto & message,
                    const std::string & senderHost,
                    std::uint16_t senderPort)
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
    using Frame = networking::internal::Frame;

    Networking & net;
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
}

#endif //NETWORKINGLIB_DATAGRAMRECEIVER_H
