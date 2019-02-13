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
#ifndef ASIONET_TESTMESSAGE_H
#define ASIONET_TESTMESSAGE_H

#include <cstdint>
#include <vector>
#include "../include/asionet/Message.h"

namespace protocol
{

using Id = std::uint32_t;
using MessageType = std::uint8_t;
using Value = std::uint32_t;

namespace messageTypes
{
constexpr MessageType REQUEST = 0x02;
constexpr MessageType RESPONSE = 0x03;
}

class TestMessage
{
public:
    TestMessage() = default;

    TestMessage(Id id, MessageType messageType, Value value)
        : id(id)
          , messageType(messageType)
          , value(value)
    {}

    static TestMessage request(Id id)
    { return TestMessage{id, messageTypes::REQUEST, 0}; }

    static TestMessage response(Id id, Value value)
    { return TestMessage{id, messageTypes::RESPONSE, value}; }

    Id getId() const
    { return id; }

    MessageType getMessageType() const
    { return messageType; }

    Value getValue() const
    { return value; }

private:
    Id id;
    MessageType messageType;
    Value value;
};

}

namespace asionet
{
namespace message
{

template<>
struct Encoder<protocol::TestMessage>
{
    void operator()(const protocol::TestMessage & message, std::string & data) const
    {
        using namespace protocol;

        data = std::string(9, '\0');

        auto id = message.getId();
        auto messageType = message.getMessageType();
        auto value = message.getValue();

        data[0] = (std::uint8_t) (id & 0x000000ff);
        data[1] = (std::uint8_t) ((id & 0x0000ff00) >> 8);
        data[2] = (std::uint8_t) ((id & 0x00ff0000) >> 16);
        data[3] = (std::uint8_t) ((id & 0xff000000) >> 24);
        data[4] = (std::uint8_t) (messageType);
        data[5] = (std::uint8_t) (value & 0x000000ff);
        data[6] = (std::uint8_t) ((value & 0x0000ff00) >> 8);
        data[7] = (std::uint8_t) ((value & 0x00ff0000) >> 16);
        data[8] = (std::uint8_t) ((value & 0xff000000) >> 24);
    }
};

template<>
struct Decoder<protocol::TestMessage>
{
    template<typename ConstBuffer>
    void operator()(const ConstBuffer & buffer, protocol::TestMessage & message) const
    {
        using namespace protocol;

        auto size = buffer.size();

        Id id{0};
        MessageType messageType{0};
        Value value{0};

        id += ((Id) buffer[0]);
        id += ((Id) buffer[1]) << 8;
        id += ((Id) buffer[2]) << 16;
        id += ((Id) buffer[3]) << 24;
        messageType += (MessageType) buffer[4];
        value += ((Value) buffer[5]);
        value += ((Value) buffer[6]) << 8;
        value += ((Value) buffer[7]) << 16;
        value += ((Value) buffer[8]) << 24;

        message = TestMessage{id, messageType, value};
    }
};

}
}

#endif //ASIONET_TESTMESSAGE_H
