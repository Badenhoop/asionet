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
	: public std::enable_shared_from_this<DatagramSender<Message>>
{
private:
	struct PrivateTag
	{
	};

public:
	using Ptr = std::shared_ptr<DatagramSender>;

	using SendHandler = std::function<void(const error::ErrorCode & error)>;

	static Ptr create(asionet::Context & context)
	{
		return std::make_shared<DatagramSender>(PrivateTag{}, context);
	}

	DatagramSender(PrivateTag, asionet::Context & context)
		: context(context)
		  , socket(context)
		  , queuedExecutor(utils::QueuedExecutor::create(context))
	{}

	void asyncSend(std::shared_ptr<Message> message,
	               const std::string & ip,
	               std::uint16_t port,
	               const time::Duration & timeout,
	               const SendHandler & handler = [](auto && ...) {})
	{
		auto self = this->shared_from_this();
		auto asyncOperation = [self](auto && ... args)
		{ self->asyncSendOperation(std::forward<decltype(args)>(args)...); };
		queuedExecutor->execute(asyncOperation, handler, message, ip, port, timeout);
	}

	void stop()
	{
		closeable::Closer<Socket>::close(socket);
		queuedExecutor->reset();
	}

private:
	using Udp = boost::asio::ip::udp;
	using Socket = Udp::socket;

	asionet::Context & context;
	Socket socket;
	utils::QueuedExecutor::Ptr queuedExecutor;

	void asyncSendOperation(const SendHandler & handler,
	                        std::shared_ptr<Message> message,
	                        const std::string & ip,
	                        std::uint16_t port,
	                        const time::Duration & timeout)
	{
		auto self = this->shared_from_this();
		setupSocket();

		asionet::message::asyncSendDatagram(
			context, socket, *message, ip, port, timeout,
			[self, handler](const auto & error)
			{
				handler(error);
			});
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
