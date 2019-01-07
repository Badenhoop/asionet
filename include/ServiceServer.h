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
		  , overrideOperation(context, [this] { this->stopOperation(); })
	{}

	void advertiseService(RequestReceivedHandler requestReceivedHandler,
	                      time::Duration receiveTimeout = std::chrono::seconds(60),
	                      time::Duration sendTimeout = std::chrono::seconds(10))
	{
		auto asyncOperation = [this](auto && ... args)
		{
			this->advertiseServiceOperation(std::forward<decltype(args)>(args)...);
		};
		overrideOperation.dispatch(asyncOperation, requestReceivedHandler, receiveTimeout, sendTimeout);
	}

	void stop()
	{
		stopOperation();
		overrideOperation.cancelPendingOperation();
	}

private:
	using Tcp = boost::asio::ip::tcp;
	using Socket = Tcp::socket;
	using Acceptor = Tcp::acceptor;
	using Frame = asionet::internal::Frame;

	struct AcceptState
	{
		AcceptState(ServiceServer<Service> & server,
		            RequestReceivedHandler && requestReceivedHandler,
		            time::Duration && receiveTimeout,
		            time::Duration && sendTimeout)
			: requestReceivedHandler(std::move(requestReceivedHandler))
			  , receiveTimeout(std::move(receiveTimeout))
			  , sendTimeout(std::move(sendTimeout))
			  , finishedNotifier(server.overrideOperation)
		{}

		RequestReceivedHandler requestReceivedHandler;
		time::Duration receiveTimeout;
		time::Duration sendTimeout;
		utils::OverrideOperation::FinishedOperationNotifier finishedNotifier;
	};

	struct ServiceState
	{
		using Ptr = std::shared_ptr<ServiceState>;

		ServiceState(ServiceServer<Service> & server, const AcceptState & acceptState)
			: socket(server.context)
			  , buffer(server.maxMessageSize + internal::Frame::HEADER_SIZE)
			  , requestReceivedHandler(acceptState.requestReceivedHandler)
			  , receiveTimeout(acceptState.receiveTimeout)
			  , sendTimeout(acceptState.sendTimeout)
		{}

		Socket socket;
		boost::asio::streambuf buffer;
		RequestReceivedHandler requestReceivedHandler;
		time::Duration receiveTimeout;
		time::Duration sendTimeout;
	};

	asionet::Context & context;
	std::uint16_t bindingPort;
	Acceptor acceptor;
	std::size_t maxMessageSize;
	std::atomic<bool> running{false};
	utils::OverrideOperation overrideOperation;

	void advertiseServiceOperation(RequestReceivedHandler & requestReceivedHandler,
	                               time::Duration & receiveTimeout,
	                               time::Duration & sendTimeout)
	{
		running = true;
		auto acceptState = std::make_shared<AcceptState>(
			*this, std::move(requestReceivedHandler), std::move(receiveTimeout), std::move(sendTimeout));
		accept(acceptState);
	}

	void accept(std::shared_ptr<AcceptState> & acceptState)
	{
		if (!acceptor.is_open())
			acceptor = Acceptor{context, Tcp::endpoint{Tcp::v4(), bindingPort}};

		auto serviceState = std::make_shared<ServiceState>(*this, *acceptState);

		// keep reference due to std::move()
		auto & socketRef = serviceState->socket;

		acceptor.async_accept(
			socketRef,
			[this, acceptState = std::move(acceptState), serviceState = std::move(serviceState)]
				(const auto & acceptError) mutable
			{
				if (!running)
					return;

				if (!acceptError)
				{
					auto & socketRef = serviceState->socket;
					auto & bufferRef = serviceState->buffer;
					auto & receiveTimeoutRef = serviceState->receiveTimeout;

					asionet::message::asyncReceive<RequestMessage>(
						context, socketRef, bufferRef, receiveTimeoutRef,
						[this, serviceState = std::move(serviceState)](const auto & errorCode, const auto & request)
						{
							// If a receive has timed out we treat it like we've never
							// received any message (and therefor we do not call the handler).
							if (errorCode)
								return;

							Endpoint clientEndpoint{serviceState->socket.remote_endpoint().address().to_string(),
							                        serviceState->socket.remote_endpoint().port()};
							ResponseMessage response;
							serviceState->requestReceivedHandler(clientEndpoint, request, response);

							auto & socketRef = serviceState->socket;
							auto & sendTimeoutRef = serviceState->sendTimeout;

							asionet::message::asyncSend(
								context, socketRef, response, sendTimeoutRef,
								[this, serviceState = std::move(serviceState)](const auto & errorCode)
								{
									// We cannot be sure that the message is going to be received at the other side anyway,
									// so we don't handle anything sending-wise.
								});
						});
				}

				// The next accept event will be put on the event queue.
				this->accept(acceptState);
			});
	}

	void stopOperation()
	{
		running = false;
		closeable::Closer<Acceptor>::close(acceptor);
	}
};

}

#endif //PROTOCOL_ProtocolNETWORKSERVICESERVER_H
