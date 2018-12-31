//
// Created by philipp on 01.12.17.
//

#ifndef PROTOCOL_ProtocolNETWORKSERVICESERVER_H
#define PROTOCOL_ProtocolNETWORKSERVICESERVER_H

#include "Message.h"
#include "Networking.h"

namespace asionet
{
namespace service
{

/**
 * Note that Service::ResponseMessage has to be default-constructable.
 * @tparam Service
 */
template<typename Service>
class Server : public std::enable_shared_from_this<Server<Service>>
               , public Busyable
{
private:
    struct PrivateTag
    {
    };

public:
    using RequestMessage = typename Service::RequestMessage;
    using ResponseMessage = typename Service::ResponseMessage;

    using Ptr = std::shared_ptr<Server<Service>>;
    using Endpoint = Resolver::Endpoint;
    using RequestReceivedHandler = std::function<void(const Endpoint & clientEndpoint,
                                                      const std::shared_ptr<RequestMessage> & requestMessage,
                                                      ResponseMessage & response)>;

    static Ptr create(Networking & net, uint16_t bindingPort, std::size_t maxMessageSize = 512)
    {
        return std::make_shared<Server<Service>>(PrivateTag{}, net, bindingPort, maxMessageSize);
    }

    // Should not be used outside.
    Server(PrivateTag,
           Networking & net,
           uint16_t bindingPort,
           std::size_t maxMessageSize)
        : net(net)
          , bindingPort(bindingPort)
          , acceptor(net.getIoService())
          , maxMessageSize(maxMessageSize)
    {}

    void advertiseService(const RequestReceivedHandler & requestReceivedHandler)
    {
        auto self = this->shared_from_this();
        auto state = std::make_shared<AdvertiseState>(self, requestReceivedHandler);
        running = true;
        accept(std::move(state));
    }

    void stop()
    {
        running = false;
        closeable::Closer<Acceptor>::close(acceptor);
    }

private:
    using Tcp = boost::asio::ip::tcp;
    using Socket = Tcp::socket;
    using Acceptor = Tcp::acceptor;
    using Frame = asionet::internal::Frame;

    struct AdvertiseState
    {
        using Ptr = std::shared_ptr<AdvertiseState>;

        AdvertiseState(Server<Service>::Ptr self,
                       const RequestReceivedHandler & requestReceivedHandler)
            : self(self)
              , lock(*self)
              , requestReceivedHandler(requestReceivedHandler)
        {}

        Server<Service>::Ptr self;
        BusyLock lock;
        RequestReceivedHandler requestReceivedHandler;
    };

    struct HandleRequestState
    {
        using Ptr = std::shared_ptr<HandleRequestState>;

        HandleRequestState(Server<Service>::Ptr self,
                           const RequestReceivedHandler & requestReceivedHandler)
            : self(self)
              , socket(self->net.getIoService())
              , requestReceivedHandler(requestReceivedHandler)
              , buffer(self->maxMessageSize + internal::Frame::HEADER_SIZE)
        {}

        Server<Service>::Ptr self;
        Socket socket;
        RequestReceivedHandler requestReceivedHandler;
        boost::asio::streambuf buffer;
    };

    Networking & net;
    std::uint16_t bindingPort;
    Acceptor acceptor;
    std::size_t maxMessageSize;
    std::atomic<bool> running{false};

    void accept(typename AdvertiseState::Ptr advertiseState)
    {
        if (!acceptor.is_open())
            acceptor = Acceptor(net.getIoService(), Tcp::endpoint{Tcp::v4(), bindingPort});

        auto handleRequestState = std::make_shared<HandleRequestState>(
            advertiseState->self, advertiseState->requestReceivedHandler);

        acceptor.async_accept(
            handleRequestState->socket,
            [advertiseState, handleRequestState](const auto & acceptError)
            {
                if (!advertiseState->self->running)
                    return;

                if (!acceptError)
                {
                    using namespace std::chrono_literals;

                    asionet::message::asyncReceive<RequestMessage>(
                        handleRequestState->self->net, handleRequestState->socket, handleRequestState->buffer, 10s,
                        [handleRequestState](const auto & errorCode, const auto & request)
                        {
                            // If a receive has timed out we treat it like we've never
                            // received any message (and therefor we do not call the handler).
                            if (errorCode)
                                return;

                            Endpoint clientEndpoint{handleRequestState->socket.remote_endpoint().address().to_string(),
                                                    handleRequestState->socket.remote_endpoint().port()};
                            ResponseMessage response;
                            handleRequestState->requestReceivedHandler(clientEndpoint, request, response);

                            asionet::message::asyncSend(
                                handleRequestState->self->net, handleRequestState->socket, response, 5s,
                                [handleRequestState](const auto & errorCode)
                                {
                                    // We cannot be sure that the message is going to be received at the other side anyway,
                                    // so we don't handle anything sending-wise.
                                });
                        });
                }

                // The next accept event will be put on the event queue.
                advertiseState->self->accept(std::move(advertiseState));
            });
    }
};

}
}

#endif //PROTOCOL_ProtocolNETWORKSERVICESERVER_H
