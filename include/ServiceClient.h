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
#include "QueuedExecutor.h"

namespace asionet
{

template<typename Service>
class ServiceClient
	: public std::enable_shared_from_this<ServiceClient<Service>>
{
private:
	struct PrivateTag
	{
	};

public:
	using Ptr = std::shared_ptr<ServiceClient<Service>>;

	using RequestMessage = typename Service::RequestMessage;
	using ResponseMessage = typename Service::ResponseMessage;

	using CallHandler = std::function<void(const error::ErrorCode & error,
										   const std::shared_ptr<ResponseMessage> & response)>;

	static Ptr create(asionet::Context & context, std::size_t maxMessageSize = 512)
	{
		return std::make_shared<ServiceClient<Service>>(PrivateTag{}, context, maxMessageSize);
	}

	ServiceClient(PrivateTag, asionet::Context & context, std::size_t maxMessageSize)
		: context(context)
		  , socket(context)
		  , maxMessageSize(maxMessageSize)
		  , queuedExecutor(utils::QueuedExecutor::create(context))
	{}

	void asyncCall(std::shared_ptr<RequestMessage> request,
	               const std::string & host,
	               std::uint16_t port,
	               const time::Duration & timeout,
	               const CallHandler & handler)
	{
		auto self = this->shared_from_this();
		auto asyncOperation = [self](auto && ... args)
		{ self->asyncCallOperation(std::forward<decltype(args)>(args)...); };
		queuedExecutor->execute(asyncOperation, handler, request, host, port, timeout);
	}

	void stop()
	{
		closeable::Closer<Socket>::close(socket);
		queuedExecutor->clear();
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
			: self(self)
			  , handler(handler)
			  , timeout(timeout)
			  , startTime(startTime)
			  , buffer(self->maxMessageSize + Frame::HEADER_SIZE)
			  , closer(self->socket)
		{}

		Ptr self;
		CallHandler handler;
		time::Duration timeout;
		time::TimePoint startTime;
		boost::asio::streambuf buffer;
		closeable::Closer <Socket> closer;
	};

	asionet::Context & context;
	Socket socket;
	std::size_t maxMessageSize;
	utils::QueuedExecutor::Ptr queuedExecutor;

	void asyncCallOperation(const CallHandler & handler,
		                    std::shared_ptr<RequestMessage> request,
		                    const std::string & host,
		                    std::uint16_t port,
		                    const time::Duration & timeout)
	{
		auto self = this->shared_from_this();
		// Container for our variables which are needed for the subsequent asynchronous calls to connect, receive and send.
		// When 'state' goes out of scope, it does cleanup.
		auto state = std::make_shared<AsyncState>(
			self, handler, timeout, time::now());

		newSocket();

		// Connect to server.
		asionet::socket::asyncConnect(
			context, socket, host, port, state->timeout,
			[state, request](const auto & error)
			{
				if (error)
				{
					std::shared_ptr<ResponseMessage> noResponse;
					state->handler(error, noResponse);
					return;
				}

				ServiceClient<Service>::updateTimeout(state->timeout, state->startTime);

				// Send the request.
				asionet::message::asyncSend(
					state->self->context, state->self->socket, *request, state->timeout,
					[state](const auto & error)
					{
						if (error)
						{
							std::shared_ptr<ResponseMessage> noResponse;
							state->handler(error, noResponse);
							return;
						}

						ServiceClient<Service>::updateTimeout(state->timeout, state->startTime);

						// Receive the response.
						asionet::message::asyncReceive<ResponseMessage>(
							state->self->context, state->self->socket, state->buffer, state->timeout,
							[state](auto const & error, const auto & response)
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
