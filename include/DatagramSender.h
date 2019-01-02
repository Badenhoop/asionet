//
// Created by philipp on 29.12.17.
//

#ifndef NETWORKINGLIB_DATAGRAMSENDER_H
#define NETWORKINGLIB_DATAGRAMSENDER_H

#include "Stream.h"
#include "Message.h"
#include "Utils.h"

namespace asionet
{

template<typename Message>
class DatagramSender
    : public std::enable_shared_from_this<DatagramSender<Message>>
      , public Busyable
{
private:
    struct PrivateTag
    {
    };

public:
    using Ptr = std::shared_ptr<DatagramSender>;

    using SendHandler = std::function<void(const error::ErrorCode & error)>;

    static Ptr create(asionet::Context & context)
    {
        return std::make_shared<DatagramSender>(PrivateTag{}, context);
    }

    DatagramSender(PrivateTag, asionet::Context & context)
        : context(context)
          , socket(context)
    {}

    void send(const Message & message,
              const std::string & ip,
              std::uint16_t port,
              const time::Duration & timeout)
    {
        BusyLock busyLock{*this};
        setupSocket();
        asionet::message::sendDatagram(context, socket, message, ip, port, timeout);
    }

    void asyncSend(const Message & message,
                   const std::string & ip,
                   std::uint16_t port,
                   const time::Duration & timeout,
                   const SendHandler & handler)
    {
        auto self = this->shared_from_this();
        auto state = std::make_shared<AsyncState>(self, handler);
        setupSocket();

        asionet::message::asyncSendDatagram(
            context, socket, message, ip, port, timeout,
            [state](const auto & error)
            {
                state->busyLock.unlock();
                state->handler(error);
            });
    }

    bool isSending() const noexcept
    {
        return isBusy();
    }

    void stop()
    {
        closeable::Closer<Socket>::close(socket);
    }

private:
    using Udp = boost::asio::ip::udp;
    using Socket = Udp::socket;

    asionet::Context & context;
    Socket socket;

    void setupSocket()
    {
        if (socket.is_open())
            return;

        socket.open(Udp::v4());
        socket.set_option(boost::asio::socket_base::broadcast{true});
    }

    struct AsyncState
    {
        AsyncState(Ptr self, const SendHandler & handler)
            : busyLock(*self)
              , self(self)
              , handler(handler)
        {}

        BusyLock busyLock;
        Ptr self;
        SendHandler handler;
    };
};

}

#endif //NETWORKINGLIB_DATAGRAMSENDER_H
