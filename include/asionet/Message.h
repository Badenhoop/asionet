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
#ifndef ASIONET_MESSAGE_H
#define ASIONET_MESSAGE_H

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
using ReceiveHandler = std::function<void(const error::Error & code, Message & message)>;

using SendToHandler = std::function<void(const error::Error & code)>;

template<typename Message>
using ReceiveFromHandler = std::function<
	void(const error::Error & code,
	     Message & message,
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
	template<typename ConstBuffer>
	void operator()(const ConstBuffer & buffer, std::string & message) const
	{ message = std::string{buffer.begin(), buffer.end()}; }
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

template<typename Message, typename ConstBuffer>
bool decode(const ConstBuffer & buffer, Message & message)
{
	try
	{
		Decoder<Message>{}(buffer, message);
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
               SendHandler handler)
{
	auto data = std::make_shared<std::string>();
	if (!internal::encode(message, *data))
	{
		stream.get_executor().context().post(
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
	asionet::stream::asyncRead(
		stream, buffer, timeout,
		[handler = std::move(handler)](const auto & errorCode, const auto & constBuffer)
		{
			Message message;
			if (!internal::decode(constBuffer, message))
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
	auto data = std::make_shared<std::string>();
	if (!internal::encode(message, *data))
	{
		socket.get_executor().context().post(
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
	asionet::socket::asyncReceiveFrom(
		socket, buffer, timeout,
		[handler = std::move(handler)](const auto & error, const auto & constBuffer, const auto & senderEndpoint)
		{
			Message message;
			if (!internal::decode(constBuffer, message))
			{
				handler(error::decoding, message, senderEndpoint);
				return;
			}
			handler(error, message, senderEndpoint);
		});
}

}
}


#endif //ASIONET_MESSAGE_H
