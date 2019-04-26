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
#ifndef ASIONET_DATAGRAMSENDER_H
#define ASIONET_DATAGRAMSENDER_H

#include "Stream.h"
#include "Message.h"
#include "Utils.h"
#include "AsyncOperationManager.h"

namespace asionet
{

template<typename Message>
class DatagramSender
{
public:
	using SendHandler = std::function<void(const error::Error & error)>;
	using Protocol = boost::asio::ip::udp;
	using Endpoint = Protocol::endpoint;
	using Socket = Protocol::socket;

	explicit DatagramSender(asionet::Context & context)
		: context(context)
		  , socket(context)
		  , operationManager(context, [this] { this->cancelOperation(); })
	{}

	void asyncSend(const Message & message,
	               const std::string & ip,
	               std::uint16_t port,
	               time::Duration timeout,
	               SendHandler handler)
	{
		asyncSend(message, Endpoint{boost::asio::ip::address::from_string(ip), port}, timeout, handler);
	}

	void asyncSend(const Message & message,
	               Endpoint endpoint,
				   time::Duration timeout,
				   SendHandler handler)
	{
		auto data = std::make_shared<std::string>();
		if (!message::internal::encode(message, *data))
		{
			context.post(
				[handler] { handler(error::encoding); });
			return;
		}

		auto asyncOperation = [this](auto && ... args)
		{ this->asyncSendOperation(std::forward<decltype(args)>(args)...); };
		operationManager.startOperation(asyncOperation, data, endpoint, timeout, handler);
	}

	void cancel()
	{
		operationManager.cancelOperation();
	}

private:
	asionet::Context & context;
	Socket socket;
	AsyncOperationManager<PendingOperationQueue> operationManager;

	struct AsyncState
	{
		AsyncState(DatagramSender<Message> & sender,
		           std::shared_ptr<std::string> && data,
		           SendHandler && handler)
			: data(std::move(data))
			  , handler(std::move(handler))
			  , finishedNotifier(sender.operationManager)
		{}

		std::shared_ptr<std::string> data;
		SendHandler handler;
		AsyncOperationManager<PendingOperationQueue>::FinishedOperationNotifier finishedNotifier;
	};

	void cancelOperation()
	{
		closeable::Closer<Socket>::close(socket);
	}

	void asyncSendOperation(std::shared_ptr<std::string> & data,
	                        Endpoint & endpoint,
	                        time::Duration & timeout,
	                        SendHandler & handler)
	{
		setupSocket();

		// keep reference because of std::move()
		auto & dataRef = *data;

		auto state = std::make_shared<AsyncState>(*this, std::move(data), std::move(handler));

		asionet::socket::asyncSendTo(
			socket, dataRef, endpoint, timeout,
			[this, state = std::move(state)](const auto & error)
			{
				state->finishedNotifier.notify();
				state->handler(error);
			});
	}

	void setupSocket()
	{
		if (socket.is_open())
			return;

		socket.open(Protocol::v4());
		socket.set_option(boost::asio::socket_base::broadcast{true});
	}
};

}

#endif //ASIONET_DATAGRAMSENDER_H
