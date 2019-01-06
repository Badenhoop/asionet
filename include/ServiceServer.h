//
// Created by philipp on 01.12.17.
//

#ifndef PROTOCOL_ProtocolNETWORKSERVICESERVER_H
#define PROTOCOL_ProtocolNETWORKSERVICESERVER_H

#include "Message.h"
#include "Context.h"

namespace asionet
{

/**
 * Note that Service::ResponseMessage has to be default-constructable.
 * @tparam Service
 */
template<typename Service>
class ServiceServer
{
public:
    using RequestMessage = typename Service::RequestMessage;
    using ResponseMessage = typename Service::ResponseMessage;

    using Endpoint = Resolver::Endpoint;
    using RequestReceivedHandler = std::function<void(const Endpoint & clientEndpoint,
                                                      const std::shared_ptr<RequestMessage> & requestMessage,
                                                      ResponseMessage & response)>;
    ServiceServer(asionet::Context & context,
                  uint16_t bindingPort,
                  std::size_t maxMessageSize = 512)
        : context(context)
          , bindingPort(bindingPort)
          , acceptor(context)
          , maxMessageSize(maxMessageSize)
    {}

    void advertiseService(const RequestReceivedHandler & requestReceivedHandler)
    {
        running = true;
        accept(requestReceivedHandler);
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

    struct HandleRequestState
    {
        using Ptr = std::shared_ptr<HandleRequestState>;

        HandleRequestState(ServiceServer<Service> & server,
                           const RequestReceivedHandler & requestReceivedHandler,
                           const time::Duration & receiveTimeout,
                           const time::Duration & sendTimeout)
            : socket(server.context)
              , requestReceivedHandler(requestReceivedHandler)
              , buffer(server.maxMessageSize + internal::Frame::HEADER_SIZE)
              , receiveTimeout(receiveTimeout)
              , sendTimeout(sendTimeout)
        {}

        Socket socket;
        RequestReceivedHandler requestReceivedHandler;
        boost::asio::streambuf buffer;
        time::Duration receiveTimeout;
        time::Duration sendTimeout;
    };

    asionet::Context & context;
    std::uint16_t bindingPort;
    Acceptor acceptor;
    std::size_t maxMessageSize;
    std::atomic<bool> running{false};

    void accept(const RequestReceivedHandler & requestReceivedHandler,
                const time::Duration & receiveTimeout = std::chrono::seconds(60),
                const time::Duration & sendTimeout = std::chrono::seconds(10))
    {
        if (!acceptor.is_open())
            acceptor = Acceptor(context, Tcp::endpoint{Tcp::v4(), bindingPort});

        auto state = std::make_shared<HandleRequestState>(*this, requestReceivedHandler, receiveTimeout, sendTimeout);

        // keep reference due to std::move()
        auto & socketRef = state->socket;

        acceptor.async_accept(
            socketRef,
            [this, requestReceivedHandler, state = std::move(state)](const auto & acceptError)
            {
                if (!running)
                    return;

                if (!acceptError)
                {
                    auto & socketRef = state->socket;
                    auto & bufferRef = state->buffer;
                    auto & receiveTimeoutRef = state->receiveTimeout;

                    asionet::message::asyncReceive<RequestMessage>(
                        context, socketRef, bufferRef, receiveTimeoutRef,
                        [this, state = std::move(state)](const auto & errorCode, const auto & request)
                        {
                            // If a receive has timed out we treat it like we've never
                            // received any message (and therefor we do not call the handler).
                            if (errorCode)
                                return;

                            Endpoint clientEndpoint{state->socket.remote_endpoint().address().to_string(),
                                                    state->socket.remote_endpoint().port()};
                            ResponseMessage response;
                            state->requestReceivedHandler(clientEndpoint, request, response);

                            auto & socketRef = state->socket;
                            auto & sendTimeoutRef = state->sendTimeout;

                            asionet::message::asyncSend(
                                context, socketRef, response, sendTimeoutRef,
                                [this, state = std::move(state)](const auto & errorCode)
                                {
                                    // We cannot be sure that the message is going to be received at the other side anyway,
                                    // so we don't handle anything sending-wise.
                                });
                        });
                }

                // The next accept event will be put on the event queue.
                this->accept(requestReceivedHandler, state->receiveTimeout, state->sendTimeout);
            });
    }
};

}

#endif //PROTOCOL_ProtocolNETWORKSERVICESERVER_H
