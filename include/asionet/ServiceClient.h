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
#ifndef ASIONET_SERVICECLIENT_H
#define ASIONET_SERVICECLIENT_H

#include <boost/asio/io_service.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/connect.hpp>
#include "Message.h"
#include "Utils.h"
#include "Error.h"
#include "Context.h"
#include "OperationQueue.h"

namespace asionet
{

template<typename Service>
class ServiceClient
{
public:
	using RequestMessage = typename Service::RequestMessage;
	using ResponseMessage = typename Service::ResponseMessage;
	using CallHandler = std::function<void(const error::Error & error,
										   const std::shared_ptr<ResponseMessage> & response)>;
	using Protocol = boost::asio::ip::tcp;
	using EndpointIterator = Protocol::resolver::iterator;
	using Socket = Protocol::socket;
	using Frame = asionet::internal::Frame;

	ServiceClient(asionet::Context & context, std::size_t maxMessageSize = 512)
		: context(context)
		  , socket(context)
		  , maxMessageSize(maxMessageSize)
		  , operationQueue(context)
	{}

	void asyncCall(const RequestMessage & request,
	               std::string host,
	               std::uint16_t port,
	               time::Duration timeout,
	               CallHandler handler)
	{
		auto sendData = encode(request, handler);
		if (!sendData)
			return;

		auto asyncOperation = [this](auto && ... args)
		{ this->asyncCallOperation(std::forward<decltype(args)>(args)...); };
		operationQueue.dispatch(asyncOperation, sendData, host, port, timeout, handler);
	}

	void asyncCall(const RequestMessage & request,
	               EndpointIterator endpointIterator,
	               time::Duration timeout,
	               CallHandler handler)
	{
		auto sendData = encode(request, handler);
		if (!sendData)
			return;

		auto asyncOperation = [this](auto && ... args)
		{ this->asyncCallOperation(std::forward<decltype(args)>(args)...); };
		operationQueue.dispatch(asyncOperation, sendData, endpointIterator, timeout, handler);
	}

	void stop()
	{
		closeable::Closer<Socket>::close(socket);
		operationQueue.cancelQueuedOperations();
	}

private:
	// We must keep track of some variables during the async handler chain.
	struct AsyncState
	{
		AsyncState(ServiceClient<Service> & client,
			       CallHandler && handler,
		           std::shared_ptr<std::string> && sendData,
		           time::Duration && timeout,
		           time::TimePoint && startTime)
			: handler(std::move(handler))
			  , sendData(std::move(sendData))
			  , timeout(std::move(timeout))
			  , startTime(std::move(startTime))
			  , buffer(client.maxMessageSize + Frame::HEADER_SIZE)
			  , closer(client.socket)
			  , finishedNotifier(client.operationQueue)
		{}

		CallHandler handler;
		std::shared_ptr<std::string> sendData;
		time::Duration timeout;
		time::TimePoint startTime;
		boost::asio::streambuf buffer;
		closeable::Closer<Socket> closer;
		utils::OperationQueue::FinishedOperationNotifier finishedNotifier;
	};

	asionet::Context & context;
	Socket socket;
	std::size_t maxMessageSize;
	utils::OperationQueue operationQueue;

	void asyncCallOperation(std::shared_ptr<std::string> & sendData,
		                    std::string & host,
	                        std::uint16_t & port,
		                    time::Duration & timeout,
		                    CallHandler & handler)
	{
		// Container for our variables which are needed for the subsequent asynchronous calls to connect, receive and send.
		// When 'state' goes out of scope, it does cleanup.
		auto state = std::make_shared<AsyncState>(
			*this, std::move(handler), std::move(sendData), std::move(timeout), std::move(time::now()));

		newSocket();

		// keep reference due to std::move()
		auto & timeoutRef = state->timeout;

		// Connect to server.
		asionet::socket::asyncConnect(
			socket, host, port, timeoutRef,
			[this, state = std::move(state)](const auto & error) mutable
			{ this->connectHandler(state, error); });
	}

	void asyncCallOperation(std::shared_ptr<std::string> & sendData,
	                        EndpointIterator & endpointIterator,
	                        time::Duration & timeout,
	                        CallHandler & handler)
	{
		auto state = std::make_shared<AsyncState>(
			*this, std::move(handler), std::move(sendData), std::move(timeout), std::move(time::now()));

		newSocket();

		auto & timeoutRef = state->timeout;

		asionet::socket::asyncConnect(
			socket, endpointIterator, timeoutRef,
			[this, state = std::move(state)](const auto & error) mutable
			{ this->connectHandler(state, error); });
	}

	void connectHandler(std::shared_ptr<AsyncState> & state, const error::Error & error)
	{
		if (error)
		{
			std::shared_ptr<ResponseMessage> noResponse;
			state->finishedNotifier.notify();
			state->handler(error, noResponse);
			return;
		}

		this->updateTimeout(state->timeout, state->startTime);

		auto & sendDataRef = state->sendData;
		auto & timeoutRef = state->timeout;

		// Send the request.
		asionet::stream::asyncWrite(
			socket, *sendDataRef, timeoutRef,
			[this, state = std::move(state)](const auto & error) mutable
			{ this->writeHandler(state, error); });
	}

	void writeHandler(std::shared_ptr<AsyncState> & state, const error::Error & error)
	{
		if (error)
		{
			std::shared_ptr<ResponseMessage> noResponse;
			state->finishedNotifier.notify();
			state->handler(error, noResponse);
			return;
		}

		this->updateTimeout(state->timeout, state->startTime);

		auto & bufferRef = state->buffer;
		auto & timeoutRef = state->timeout;

		// Receive the response.
		asionet::message::asyncReceive<ResponseMessage>(
			socket, bufferRef, timeoutRef,
			[this, state = std::move(state)](auto const & error, const auto & response)
			{
				state->finishedNotifier.notify();
				state->handler(error, response);
			});
	}

	static void updateTimeout(time::Duration & timeout, time::TimePoint & startTime)
	{
		auto nowTime = time::now();
		auto timeSpend = nowTime - startTime;
		startTime = nowTime;
		timeout -= timeSpend;
	}

	std::shared_ptr<std::string> encode(const RequestMessage & request, CallHandler & handler)
	{
		auto sendData = std::make_shared<std::string>();
		if (!message::internal::encode(request, *sendData))
		{
			context.post(
				[handler]
				{
					std::shared_ptr<ResponseMessage> noResponse;
					handler(error::encoding, noResponse);
				});
			return nullptr;
		}
		return sendData;
	}

	void newSocket()
	{
		if (!socket.is_open())
			socket = Socket(context);
	}
};

}


#endif //ASIONET_SERVICECLIENT_H
