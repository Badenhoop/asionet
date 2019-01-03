//
// Created by philipp on 29.12.17.
//

#ifndef NETWORKINGLIB_DATAGRAMSENDER_H
#define NETWORKINGLIB_DATAGRAMSENDER_H

#include <queue>
#include "Stream.h"
#include "Message.h"
#include "Utils.h"

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
	{}

	void asyncSend(std::shared_ptr<Message> message,
	               const std::string & ip,
	               std::uint16_t port,
	               const time::Duration & timeout,
	               const SendHandler & handler)
	{
		handlerQueueMonitor(
			[&](auto & handlerQueue)
			{
				if (handlerQueue.empty())
				{
					asyncOperation(message, ip, port, timeout, handler);
					return;
				}

				auto self = this->shared_from_this();
				handlerQueue.push(
					[self, message, ip, port, timeout, handler]
					{
						self->asyncOperation(message, ip, port, timeout, handler);
					});
			});
	}

	void stop()
	{
		closeable::Closer<Socket>::close(socket);

		handlerQueueMonitor(
			[&](auto & handlerQueue)
			{
				handlerQueue = std::queue<std::function<void()>>;
			});
	}

private:
	using Udp = boost::asio::ip::udp;
	using Socket = Udp::socket;

	asionet::Context & context;
	Socket socket;
	utils::Monitor <std::queue<std::function<void()>>> handlerQueueMonitor;

	void asyncOperation(std::shared_ptr<Message> message,
	                    const std::string & ip,
	                    std::uint16_t port,
	                    const time::Duration & timeout,
	                    const SendHandler & handler)
	{
		auto self = this->shared_from_this();
		setupSocket();

		asionet::message::asyncSendDatagram(
			context, socket, message, ip, port, timeout,
			[self, handler](const auto & error)
			{
				handler(error);

				self->handlerQueueMonitor(
					[&](auto & handlerQueue)
					{
						if (!handlerQueue.empty())
						{
							auto nextOperation = handlerQueue.pop();
							self->context.post(nextOperation);
						}
					});
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
