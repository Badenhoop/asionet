//
// Created by philipp on 22.01.19.
//

#ifndef ASIONET_BUFFER_H
#define ASIONET_BUFFER_H

#include <boost/asio/streambuf.hpp>
#include <boost/asio.hpp>

namespace asionet
{
namespace internal
{

class ConstStreamBuffer
{
public:
	using ConstIterator = boost::asio::buffers_iterator<boost::asio::streambuf::const_buffers_type>;

	ConstStreamBuffer(boost::asio::streambuf & buffer, std::size_t numBytes, std::size_t offset)
		: buffer(buffer), numBytes(numBytes), offset(offset)
	{
		assert(buffer.size() >= offset + numBytes);
	}

	char operator[](std::size_t pos) const
	{
		return ((const char *) buffer.data().data())[pos + offset];
	}

	std::size_t size() const
	{
		return numBytes;
	}

	ConstIterator begin() const
	{
		return boost::asio::buffers_begin(buffer.data()) + offset;
	}

	ConstIterator end() const
	{
		return boost::asio::buffers_begin(buffer.data()) + offset + numBytes;
	}

private:
	boost::asio::streambuf & buffer;
	std::size_t numBytes;
	std::size_t offset;
};

class ConstVectorBuffer
{
public:
	using ConstIterator = std::vector<char>::const_iterator;

	explicit ConstVectorBuffer(std::vector<char> & buffer, std::size_t numBytes, std::size_t offset)
		: buffer(buffer), numBytes(numBytes), offset(offset)
	{
		assert(buffer.size() >= offset + numBytes);
	}

	char operator[](std::size_t pos) const
	{
		return buffer[pos + offset];
	}

	std::size_t size() const
	{
		return numBytes;
	}

	ConstIterator begin() const
	{
		return buffer.begin() + offset;
	}

	ConstIterator end() const
	{
		return buffer.begin() + offset + numBytes;
	}

private:
	std::vector<char> buffer;
	std::size_t numBytes;
	std::size_t offset;
};

}
}

#endif //ASIONET_BUFFER_H
