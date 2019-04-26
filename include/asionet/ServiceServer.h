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
#ifndef ASIONET_SERVICESERVER_H
#define ASIONET_SERVICESERVER_H

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
	using Protocol = boost::asio::ip::tcp;
	using Socket = Protocol::socket;
	using Acceptor = Protocol::acceptor;
	using Frame = asionet::internal::Frame;
	using Endpoint = Protocol::endpoint;
	using RequestReceivedHandler = std::function<void(const Endpoint & clientEndpoint,
	                                                  RequestMessage & requestMessage,
	                                                  ResponseMessage & response)>;

	ServiceServer(asionet::Context & context,
	              uint16_t bindingPort,
	              std::size_t maxMessageSize = 512)
		: context(context)
		  , bindingPort(bindingPort)
		  , acceptor(context)
		  , maxMessageSize(maxMessageSize)
		  , operationManager(context, [this] { this->cancelOperation(); })
	{}

	void advertiseService(RequestReceivedHandler requestReceivedHandler,
	                      time::Duration receiveTimeout = std::chrono::seconds(60),
	                      time::Duration sendTimeout = std::chrono::seconds(10))
	{
		auto asyncOperation = [this](auto && ... args)
		{
			this->advertiseServiceOperation(std::forward<decltype(args)>(args)...);
		};
		operationManager.startOperation(asyncOperation, requestReceivedHandler, receiveTimeout, sendTimeout);
	}

	void cancel()
	{
		operationManager.cancelOperation();
	}

private:
	struct AcceptState
	{
		AcceptState(ServiceServer<Service> & server,
		            RequestReceivedHandler && requestReceivedHandler,
		            time::Duration && receiveTimeout,
		            time::Duration && sendTimeout)
			: requestReceivedHandler(std::move(requestReceivedHandler))
			  , receiveTimeout(std::move(receiveTimeout))
			  , sendTimeout(std::move(sendTimeout))
			  , finishedNotifier(server.operationManager)
		{}

		RequestReceivedHandler requestReceivedHandler;
		time::Duration receiveTimeout;
		time::Duration sendTimeout;
		AsyncOperationManager<PendingOperationReplacer>::FinishedOperationNotifier finishedNotifier;
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
	AsyncOperationManager<PendingOperationReplacer> operationManager;

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
			acceptor = Acceptor{context, Protocol::endpoint{Protocol::v4(), bindingPort}};

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

				if (!acceptError && !operationManager.isCanceled())
					this->handleService(serviceState);

				// The next accept event will be put on the event queue.
				this->accept(acceptState);
			});
	}

	bool handleService(std::shared_ptr<ServiceState> & serviceState)
	{
		auto & socketRef = serviceState->socket;
		auto & bufferRef = serviceState->buffer;
		auto & receiveTimeoutRef = serviceState->receiveTimeout;

		asionet::message::asyncReceive<RequestMessage>(
			socketRef, bufferRef, receiveTimeoutRef,
			[this, serviceState = std::move(serviceState)](const auto & errorCode, auto & request)
			{
				// If a receive has timed out we treat it like we've never
				// received any message (and therefor we do not call the handler).
				if (errorCode)
					return;

				ResponseMessage response;
				serviceState->requestReceivedHandler(serviceState->socket.remote_endpoint(), request, response);

				auto & socketRef = serviceState->socket;
				auto & sendTimeoutRef = serviceState->sendTimeout;

				asionet::message::asyncSend(
					socketRef, response, sendTimeoutRef,
					[this, serviceState = std::move(serviceState)](const auto & errorCode)
					{
						// We cannot be sure that the message is going to be received at the other side anyway,
						// so we don't handle anything sending-wise.
					});
			});
	}

	void cancelOperation()
	{
		running = false;
		closeable::Closer<Acceptor>::close(acceptor);
	}
};

}

#endif //ASIONET_SERVICESERVER_H
