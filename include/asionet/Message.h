//
// Created by philipp on 01.12.17.
//

#ifndef PROTOCOL_MESSAGES_H
#define PROTOCOL_MESSAGES_H

#include <cstdint>
#include <vector>
#include <boost/system/error_code.hpp>
#include <boost/asio/write.hpp>
#include <boost/asio/read.hpp>
#include "Stream.h"
#include "Socket.h"
#include <boost/algorithm/string/replace.hpp>

namespace asionet
{
namespace message
{

using SendHandler = std::function<void(const error::Error & code)>;

template<typename Message>
using ReceiveHandler = std::function<void(const error::Error & code, std::shared_ptr<Message> & message)>;

using SendToHandler = std::function<void(const error::Error & code)>;

template<typename Message>
using ReceiveFromHandler = std::function<
	void(const error::Error & code,
	     std::shared_ptr<Message> & message,
	     const boost::asio::ip::udp::endpoint & endpoint)>;

template<typename Message>
struct Encoder;

template<>
struct Encoder<std::string>
{
	void operator()(const std::string & message, std::string & data) const
	{ data = message; }
};

template<typename Message>
struct Decoder;

template<>
struct Decoder<std::string>
{
	std::shared_ptr<std::string> operator()(const std::string & data) const
	{ return std::make_shared<std::string>(data); }
};

namespace internal
{

template<typename Message>
bool encode(const Message & message, std::string & data)
{
	try
	{
		Encoder<Message>{}(message, data);
		return true;
	}
	catch (...)
	{
		return false;
	}
}

template<typename Message>
bool decode(const std::string & data, std::shared_ptr<Message> & message)
{
	try
	{
		message = Decoder<Message>{}(data);
		return true;
	}
	catch (...)
	{
		return false;
	}
}

}

template<typename Message, typename SyncWriteStream>
void asyncSend(SyncWriteStream & stream,
               const Message & message,
               const time::Duration & timeout,
               SendHandler handler = [] (auto && ...) {})
{
	auto & context = stream.get_executor().context();
	auto data = std::make_shared<std::string>();
	if (!internal::encode(message, *data))
	{
		context.post(
			[handler] { handler(error::encoding); });
		return;
	}

	// keep reference because of std::move()
	auto & dataRef = *data;

	asionet::stream::asyncWrite(
		stream, dataRef, timeout,
		[handler = std::move(handler), data = std::move(data)](const auto & errorCode) { handler(errorCode); });
};

template<typename Message, typename SyncReadStream>
void asyncReceive(SyncReadStream & stream,
                  boost::asio::streambuf & buffer,
                  const time::Duration & timeout,
                  ReceiveHandler<Message> handler)
{
	auto & context = stream.get_executor().context();
	asionet::stream::asyncRead(
		stream, buffer, timeout,
		[handler = std::move(handler)](const auto & errorCode, const auto & data)
		{
			std::shared_ptr<Message> message;
			if (!internal::decode(data, message))
			{
				handler(error::decoding, message);
				return;
			}
			handler(errorCode, message);
		});
};

template<typename Message, typename DatagramSocket>
void asyncSendDatagram(DatagramSocket & socket,
                       const Message & message,
                       const std::string & ip,
                       std::uint16_t port,
                       const time::Duration & timeout,
                       SendToHandler handler)
{
	using Endpoint = boost::asio::ip::udp::endpoint;
	asyncSendDatagram(socket, message, Endpoint{boost::asio::ip::address::from_string(ip), port}, timeout, handler);
}

template<typename Message, typename DatagramSocket, typename Endpoint>
void asyncSendDatagram(DatagramSocket & socket,
                       const Message & message,
                       const Endpoint & endpoint,
                       const time::Duration & timeout,
                       SendToHandler handler)
{
	auto & context = socket.get_executor().context();
	auto data = std::make_shared<std::string>();
	if (!internal::encode(message, *data))
	{
		context.post(
			[handler] { handler(error::encoding); });
		return;
	}

	// keep reference because of std::move()
	auto & dataRef = *data;

	asionet::socket::asyncSendTo(
		socket, dataRef, endpoint, timeout,
		[handler = std::move(handler), data = std::move(data)](const auto & error) { handler(error); });
}

template<typename Message, typename DatagramSocket>
void asyncReceiveDatagram(DatagramSocket & socket,
                          std::vector<char> & buffer,
                          const time::Duration & timeout,
                          ReceiveFromHandler<Message> handler)
{
	auto & context = socket.get_executor().context();
	asionet::socket::asyncReceiveFrom(
		socket, buffer, timeout,
		[handler = std::move(handler)](const auto & error, const auto & data, const auto & senderEndpoint)
		{
			std::shared_ptr<Message> message;
			if (!internal::decode(data, message))
			{
				handler(error::decoding, message, senderEndpoint);
				return;
			}
			handler(error, message, senderEndpoint);
		});
}

}
}


#endif //PROTOCOL_MESSAGES_H
