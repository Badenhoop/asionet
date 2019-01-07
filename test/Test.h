//
// Created by philipp on 02.12.17.
//

#ifndef PROTOCOL_NETWORKTEST_H_H
#define PROTOCOL_NETWORKTEST_H_H

#include <string>
#include "../include/asionet/Message.h"

namespace asionet
{
namespace test
{

class StringService
{
public:
    using RequestMessage = std::string;
    using ResponseMessage = std::string;
};

class NonCopyableMessage
{
public:
    NonCopyableMessage() = default;
    NonCopyableMessage(const NonCopyableMessage &) = delete;
    NonCopyableMessage & operator=(const NonCopyableMessage &) = delete;
    NonCopyableMessage(NonCopyableMessage &&) = delete;
    NonCopyableMessage & operator=(NonCopyableMessage &&) = delete;
};

}
}

namespace asionet
{
namespace message
{

template<>
struct Encoder<asionet::test::NonCopyableMessage>
{
    void operator()(const asionet::test::NonCopyableMessage & message, std::string & data) const
    { data = ""; }
};

template<>
struct Decoder<asionet::test::NonCopyableMessage>
{
    std::shared_ptr<asionet::test::NonCopyableMessage> operator()(const std::string & data) const
    { return std::make_shared<asionet::test::NonCopyableMessage>(); }
};

}
}

#endif //PROTOCOL_NETWORKTEST_H_H
