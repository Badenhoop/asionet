//
// Created by philipp on 02.12.17.
//

#ifndef PROTOCOL_NETWORKTEST_H_H
#define PROTOCOL_NETWORKTEST_H_H

#include <string>
#include "../include/NetworkingLib/Message.h"

namespace networking
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

void testSyncServices();

void testAsyncServices();

void testTcpClientTimeout();

void testMultipleConnections();

void testStoppingServiceServer();

void testAsyncDatagramReceiver();

void testPeriodicTimer();

void testServiceClientAsyncCallTimeout();

void testDatagramSenderAsyncSend();

void testResolver();

void testStringMessageOverDatagram();

void testStringMessageOverService();

void testServiceServerMaxMessageSize();

void testServiceClientMaxMessageSize();

void testDatagramReceiverMaxMessageSize();

void testServiceLargeTransferSize();

void testNonCopyableMessage();

}
}

namespace networking
{
namespace message
{

template<>
struct Encoder<networking::test::NonCopyableMessage>
{
    void operator()(const networking::test::NonCopyableMessage & message, std::string & data) const
    {}
};

template<>
struct Decoder<networking::test::NonCopyableMessage>
{
    void operator()(networking::test::NonCopyableMessage & message, const std::string & data) const
    {}
};

}
}

#endif //PROTOCOL_NETWORKTEST_H_H
