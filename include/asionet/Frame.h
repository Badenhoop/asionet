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
#ifndef ASIONET_FRAME_H
#define ASIONET_FRAME_H

#include <cstdint>
#include <boost/asio/buffer.hpp>
#include "Utils.h"

namespace asionet
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

#endif //ASIONET_FRAME_H
