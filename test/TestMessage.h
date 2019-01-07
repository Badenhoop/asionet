//
// Created by philipp on 01.12.17.
//

#ifndef PROTOCOL_PLATOONCREATEMESSAGE_H
#define PROTOCOL_PLATOONCREATEMESSAGE_H

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
    std::shared_ptr<protocol::TestMessage> operator()(const std::string & data) const
    {
        using namespace protocol;

        auto size = data.size();

        Id id{0};
        MessageType messageType{0};
        Value value{0};

        id += ((Id) data[0]);
        id += ((Id) data[1]) << 8;
        id += ((Id) data[2]) << 16;
        id += ((Id) data[3]) << 24;
        messageType += (MessageType) data[4];
        value += ((Value) data[5]);
        value += ((Value) data[6]) << 8;
        value += ((Value) data[7]) << 16;
        value += ((Value) data[8]) << 24;

        return std::make_shared<TestMessage>(id, messageType, value);
    }
};

}
}

#endif //PROTOCOL_PLATOONCREATEMESSAGE_H
