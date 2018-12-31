//
// Created by philipp on 17.01.18.
//

#ifndef NETWORKINGLIB_FRAME_H
#define NETWORKINGLIB_FRAME_H

#include <cstdint>
#include <boost/asio/buffer.hpp>
#include "Utils.h"

namespace networking
{
namespace internal
{

class Frame
{
public:
    static constexpr std::size_t HEADER_SIZE = 4;

    Frame(const std::uint8_t * data, std::uint32_t numDataBytes)
        : data(data), numDataBytes(numDataBytes)
    {
        utils::toBigEndian<4>(header, numDataBytes);
    }

    Frame(const Frame &) = delete;

    Frame & operator=(const Frame &) = delete;

    Frame(Frame &&) = delete;

    Frame & operator=(Frame &&) = delete;

    auto getBuffers() const
    {
        return std::vector<boost::asio::const_buffer>{
            boost::asio::buffer((const void *) header, sizeof(header)),
            boost::asio::buffer((const void *) data, numDataBytes)};
    }

    std::size_t getSize() const
    {
        return sizeof(header) + numDataBytes;
    }

private:
    std::uint32_t numDataBytes;
    std::uint8_t header[4];
    const std::uint8_t * data;
};

}
}

#endif //NETWORKINGLIB_FRAME_H
