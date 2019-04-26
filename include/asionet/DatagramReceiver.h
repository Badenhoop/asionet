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
#ifndef ASIONET_DATAGRAMRECEIVER_H
#define ASIONET_DATAGRAMRECEIVER_H

#include "Stream.h"
#include "Socket.h"
#include "Message.h"
#include "Context.h"
#include "AsyncOperationManager.h"

namespace asionet
{

template<typename Message>
class DatagramReceiver
{
public:
	using Protocol = boost::asio::ip::udp;
	using Endpoint = Protocol::endpoint;
	using Socket = Protocol::socket;
	using Frame = asionet::internal::Frame;
	using ReceiveHandler = std::function<
		void(const error::Error & error,
			 Message & message,
			 const Endpoint & senderEndpoint)>;

	DatagramReceiver(asionet::Context & context, std::uint16_t bindingPort, std::size_t maxMessageSize = 512)
		: context(context)
		  , bindingPort(bindingPort)
		  , socket(context)
		  , buffer(maxMessageSize + Frame::HEADER_SIZE)
		  , operationManager(context, [this]{ this->cancelOperation(); })
	{}

	void asyncReceive(time::Duration timeout, ReceiveHandler handler)
	{
		auto asyncOperation = [this](auto && ... args)
		{ this->asyncReceiveOperation(std::forward<decltype(args)>(args)...); };
		operationManager.startOperation(asyncOperation, timeout, handler);
	}

	void cancel()
	{
		operationManager.cancelOperation();
	}

private:
	asionet::Context & context;
	std::uint16_t bindingPort;
	Socket socket;
	std::vector<char> buffer;
	AsyncOperationManager<PendingOperationReplacer> operationManager;

	struct AsyncState
	{
		AsyncState(DatagramReceiver<Message> & receiver,
		           ReceiveHandler && handler)
			: handler(std::move(handler))
			  , finishedNotifier(receiver.operationManager)
		{}

		ReceiveHandler handler;
		AsyncOperationManager<PendingOperationReplacer>::FinishedOperationNotifier finishedNotifier;
	};

	void asyncReceiveOperation(time::Duration & timeout, ReceiveHandler & handler)
	{
		setupSocket();

		auto state = std::make_shared<AsyncState>(*this, std::move(handler));

		message::asyncReceiveDatagram<Message>(
			socket, buffer, timeout,
			[this, state = std::move(state)] (const auto & error, auto & message, const auto & senderEndpoint)
			{
				if (operationManager.isCanceled())
					return;

				state->finishedNotifier.notify();
				state->handler(error, message, senderEndpoint);
			});
	}

	void cancelOperation()
	{
		closeable::Closer<Socket>::close(socket);
	}

	void setupSocket()
	{
		if (socket.is_open())
			return;

		socket.open(Protocol::v4());
		socket.set_option(boost::asio::socket_base::reuse_address{true});
		socket.set_option(boost::asio::socket_base::broadcast{true});
		socket.bind(Endpoint(Protocol::v4(), bindingPort));
	}
};

}

#endif //ASIONET_DATAGRAMRECEIVER_H
