//
// Created by philipp on 29.12.17.
//

#ifndef NETWORKINGLIB_DATAGRAMSENDER_H
#define NETWORKINGLIB_DATAGRAMSENDER_H

#include "Stream.h"
#include "Message.h"
#include "Utils.h"
#include "QueuedExecutor.h"

namespace asionet
{

template<typename Message>
class DatagramSender
{
public:
	using SendHandler = std::function<void(const error::ErrorCode & error)>;

	explicit DatagramSender(asionet::Context & context)
		: context(context)
		  , socket(context)
		  , queuedExecutor(context)
	{}

	void asyncSend(const Message & message,
	               const std::string & ip,
	               std::uint16_t port,
	               const time::Duration & timeout,
	               const SendHandler & handler = [](auto && ...) {})
	{
		auto data = std::make_shared<std::string>();
		if (!message::internal::encode(message, *data))
		{
			context.post(
				[handler] { handler(error::codes::ENCODING); });
			return;
		}

		auto asyncOperation = [this](auto && ... args)
		{ this->asyncSendOperation(std::forward<decltype(args)>(args)...); };
		queuedExecutor.execute(asyncOperation, handler, data, ip, port, timeout);
	}

	void stop()
	{
		closeable::Closer<Socket>::close(socket);
		queuedExecutor.clear();
	}

private:
	using Udp = boost::asio::ip::udp;
	using Socket = Udp::socket;

	asionet::Context & context;
	Socket socket;
	utils::QueuedExecutor queuedExecutor;

	void asyncSendOperation(const SendHandler & handler,
	                        std::shared_ptr<std::string> & data,
	                        const std::string & ip,
	                        std::uint16_t port,
	                        const time::Duration & timeout)
	{
		setupSocket();

		// keep reference because of std::move()
		auto & dataRef = *data;

		asionet::socket::asyncSendTo(
			context, socket, dataRef, ip, port, timeout,
			[handler, data = std::move(data)](const auto & error) { handler(error); });
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
