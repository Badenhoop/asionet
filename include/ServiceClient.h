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
#include "Utils.h"
#include "Error.h"
#include "Busyable.h"
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

	ServiceClient(asionet::Context & context, std::size_t maxMessageSize = 512)
		: context(context)
		  , socket(context)
		  , maxMessageSize(maxMessageSize)
		  , operationQueue(context)
	{}

	void asyncCall(const RequestMessage & request,
	               const std::string & host,
	               std::uint16_t port,
	               const time::Duration & timeout,
	               const CallHandler & handler)
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
			return;
		}

		auto asyncOperation = [this](auto && ... args)
		{ this->asyncCallOperation(std::forward<decltype(args)>(args)...); };
		operationQueue.execute(asyncOperation, handler, sendData, host, port, timeout);
	}

	void stop()
	{
		closeable::Closer<Socket>::close(socket);
		operationQueue.clear();
	}

private:
	using Socket = boost::asio::ip::tcp::socket;
	using Frame = asionet::internal::Frame;

	// We must keep track of some variables during the async handler chain.
	struct AsyncState
	{
		AsyncState(ServiceClient<Service> & client,
			       const CallHandler & handler,
		           std::shared_ptr<std::string> & sendData,
		           time::Duration timeout,
		           time::TimePoint startTime)
			: handler(handler)
			  , sendData(std::move(sendData))
			  , timeout(timeout)
			  , startTime(startTime)
			  , buffer(client.maxMessageSize + Frame::HEADER_SIZE)
			  , closer(client.socket)
		{}

		CallHandler handler;
		std::shared_ptr<std::string> sendData;
		time::Duration timeout;
		time::TimePoint startTime;
		boost::asio::streambuf buffer;
		closeable::Closer<Socket> closer;
	};

	asionet::Context & context;
	Socket socket;
	std::size_t maxMessageSize;
	utils::OperationQueue operationQueue;

	void asyncCallOperation(const CallHandler & handler,
		                    std::shared_ptr<std::string> & sendData,
		                    const std::string & host,
		                    std::uint16_t port,
		                    const time::Duration & timeout)
	{
		// Container for our variables which are needed for the subsequent asynchronous calls to connect, receive and send.
		// When 'state' goes out of scope, it does cleanup.
		auto state = std::make_shared<AsyncState>(
			*this, handler, sendData, timeout, time::now());

		newSocket();

		// keep reference due to std::move()
		auto & timeoutRef = state->timeout;

		// Connect to server.
		asionet::socket::asyncConnect(
			context, socket, host, port, timeoutRef,
			[this, state = std::move(state)](const auto & error)
			{
				if (error)
				{
					std::shared_ptr<ResponseMessage> noResponse;
					state->handler(error, noResponse);
					return;
				}

				this->updateTimeout(state->timeout, state->startTime);

				auto & sendDataRef = state->sendData;
				auto & timeoutRef = state->timeout;

				// Send the request.
				asionet::stream::asyncWrite(
					context, socket, *sendDataRef, timeoutRef,
					[this, state = std::move(state)](const auto & error)
					{
						if (error)
						{
							std::shared_ptr<ResponseMessage> noResponse;
							state->handler(error, noResponse);
							return;
						}

						this->updateTimeout(state->timeout, state->startTime);

						auto & bufferRef = state->buffer;
						auto & timeoutRef = state->timeout;

						// Receive the response.
						asionet::message::asyncReceive<ResponseMessage>(
							context, socket, bufferRef, timeoutRef,
							[this, state = std::move(state)](auto const & error, const auto & response)
							{
								state->handler(error, response);
							});
					});
			});
	}

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
			socket = Socket(context);
	}
};

}


#endif //PROTOCOL_TCPNETWORKSERVICECLIENT_H
