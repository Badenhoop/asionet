//
// Created by philipp on 29.12.17.
//

#ifndef NETWORKINGLIB_DATAGRAMSENDER_H
#define NETWORKINGLIB_DATAGRAMSENDER_H

#include "Stream.h"
#include "Message.h"
#include "Utils.h"
#include "OperationQueue.h"

namespace asionet
{

template<typename Message>
class DatagramSender
{
public:
	using SendHandler = std::function<void(const error::Error & error)>;

	explicit DatagramSender(asionet::Context & context)
		: context(context)
		  , socket(context)
		  , operationQueue(context)
	{}

	void asyncSend(const Message & message,
	               std::string ip,
	               std::uint16_t port,
	               time::Duration timeout,
	               SendHandler handler = [](auto && ...) {})
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
		operationQueue.dispatch(asyncOperation, data, ip, port, timeout, handler);
	}

	void stop()
	{
		closeable::Closer<Socket>::close(socket);
		operationQueue.stop();
	}

private:
	using Udp = boost::asio::ip::udp;
	using Socket = Udp::socket;

	asionet::Context & context;
	Socket socket;
	utils::OperationQueue operationQueue;

	struct AsyncState
	{
		AsyncState(DatagramSender<Message> & sender,
		           std::shared_ptr<std::string> && data,
		           SendHandler && handler)
			: data(std::move(data))
			  , handler(std::move(handler))
			  , notifier(sender.operationQueue)
		{}

		std::shared_ptr<std::string> data;
		SendHandler handler;
		utils::OperationQueue::FinishedOperationNotifier notifier;
	};

	void asyncSendOperation(std::shared_ptr<std::string> & data,
	                        std::string & ip,
	                        std::uint16_t & port,
	                        time::Duration & timeout,
	                        SendHandler & handler)
	{
		setupSocket();

		// keep reference because of std::move()
		auto & dataRef = *data;

		auto state = std::make_shared<AsyncState>(*this, std::move(data), std::move(handler));

		asionet::socket::asyncSendTo(
			context, socket, dataRef, ip, port, timeout,
			[this, state = std::move(state)](const auto & error)
			{ state->handler(error); });
	}

	void setupSocket()
	{
		if (socket.is_open())
			return;

		socket.open(Udp::v4());
		socket.set_option(boost::asio::socket_base::broadcast{true});
	}
};

}

#endif //NETWORKINGLIB_DATAGRAMSENDER_H
