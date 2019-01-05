//
// Created by philipp on 02.12.17.
//

#ifndef PROTOCOL_NETWORKTEST_H_H
#define PROTOCOL_NETWORKTEST_H_H

#include <string>
#include "../include/Message.h"

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

void testAsyncServices();

void testTcpClientTimeout();

void testMultipleConnections();

void testStoppingServiceServer();

void testAsyncDatagramReceiver();

void testPeriodicTimer();

void testServiceClientAsyncCallTimeout();

void testDatagramSenderAsyncSend();

void testDatagramSenderQueuedSending();

void testResolver();

void testStringMessageOverDatagram();

void testStringMessageOverService();

void testServiceServerMaxMessageSize();

void testServiceClientMaxMessageSize();

void testDatagramReceiverMaxMessageSize();

void testServiceLargeTransferSize();

void testNonCopyableMessage();

void testWorkerPool();

void testWorkSerializer();

}
}

namespace asionet
{
namespace message
{

template<>
struct Encoder<asionet::test::NonCopyableMessage>
{
    std::shared_ptr<std::string> operator()(const asionet::test::NonCopyableMessage & message) const
    { return std::make_shared<std::string>(""); }
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
