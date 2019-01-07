//
// Created by philipp on 15.01.18.
//

#ifndef NETWORKINGLIB_DATAGRAMRECEIVER_H
#define NETWORKINGLIB_DATAGRAMRECEIVER_H

#include "Stream.h"
#include "Socket.h"
#include "Message.h"
#include "Context.h"
#include "OverrideOperation.h"

namespace asionet
{

template<typename Message>
class DatagramReceiver
{
public:
	using ReceiveHandler = std::function<
		void(const error::Error & error,
		     const std::shared_ptr<Message> & message,
		     const std::string & host,
		     std::uint16_t port)>;

	DatagramReceiver(asionet::Context & context, std::uint16_t bindingPort, std::size_t maxMessageSize = 512)
		: context(context)
		  , bindingPort(bindingPort)
		  , socket(context)
		  , buffer(maxMessageSize + Frame::HEADER_SIZE)
		  , overrideOperation(context)
	{}

	void asyncReceive(time::Duration timeout, ReceiveHandler handler)
	{
		auto asyncOperation = [this](auto && ... args)
		{ this->asyncReceiveOperation(std::forward<decltype(args)>(args)...); };
		overrideOperation.dispatch(asyncOperation, timeout, handler);
	}

	void stop()
	{
		closeable::Closer<Socket>::close(socket);
		overrideOperation.stop();
	}

private:
	using Udp = boost::asio::ip::udp;
	using Endpoint = Udp::endpoint;
	using Socket = Udp::socket;
	using Frame = asionet::internal::Frame;

	asionet::Context & context;
	std::uint16_t bindingPort;
	Socket socket;
	std::vector<char> buffer;
	utils::OverrideOperation overrideOperation;

	struct AsyncState
	{
		AsyncState(DatagramReceiver<Message> & receiver,
		           ReceiveHandler && handler)
			: handler(std::move(handler))
			  , notifier(receiver.overrideOperation)
		{}

		ReceiveHandler handler;
		utils::OverrideOperation::FinishedOperationNotifier notifier;
	};

	void asyncReceiveOperation(time::Duration & timeout, ReceiveHandler & handler)
	{
		setupSocket();

		auto state = std::make_shared<AsyncState>(*this, std::move(handler));

		message::asyncReceiveDatagram<Message>(
			context, socket, buffer, timeout,
			[state = std::move(state)] (const auto & error, const auto & message, const auto & senderHost, auto senderPort)
			{
				state->handler(error, message, senderHost, senderPort);
			});
	}

	void setupSocket()
	{
		if (socket.is_open())
			return;

		socket.open(Udp::v4());
		socket.set_option(boost::asio::socket_base::reuse_address{true});
		socket.set_option(boost::asio::socket_base::broadcast{true});
		socket.bind(Endpoint(Udp::v4(), bindingPort));
	}
};

}

#endif //NETWORKINGLIB_DATAGRAMRECEIVER_H
