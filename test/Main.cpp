//
// Created by philipp on 17.12.17.
//

#include "Test.h"
#include "../include/ServiceServer.h"
#include <iostream>

int main(int argc, char ** argv)
{
    std::cout << "\ntestSyncService\n\n";
    asionet::test::testSyncServices();
    std::cout << "\ntestAsyncService\n\n";
    asionet::test::testAsyncServices();
    std::cout <<"\ntestTcpClientTimeout\n\n";
    asionet::test::testTcpClientTimeout();
    std::cout <<"\ntestMultipleConnections\n\n";
    asionet::test::testMultipleConnections();
    std::cout << "\ntestStoppingServiceServer\n\n";
    asionet::test::testStoppingServiceServer();
    std::cout << "\ntestAsyncDatagramReceiver\n\n";
    asionet::test::testAsyncDatagramReceiver();
    std::cout << "\ntestPeriodicTimer\n\n";
    asionet::test::testPeriodicTimer();
    std::cout << "\ntestServiceClientAsyncCallTimeout\n\n";
    asionet::test::testServiceClientAsyncCallTimeout();
    std::cout << "\ntestDatagramSenderAsyncSend\n\n";
    asionet::test::testDatagramSenderAsyncSend();
    std::cout << "\ntestResolver\n\n";
    asionet::test::testResolver();
    std::cout << "\ntestStringMessageOverDatagram\n\n";
    asionet::test::testStringMessageOverDatagram();
    std::cout << "\ntestStringMessageOverService\n\n";
    asionet::test::testStringMessageOverService();
    std::cout << "\ntestServiceServerMaxMessageSize\n\n";
    asionet::test::testServiceServerMaxMessageSize();
    std::cout << "\ntestServiceClientMaxMessageSize\n\n";
    asionet::test::testServiceClientMaxMessageSize();
    std::cout << "\ntestDatagramReceiverMaxMessageSize\n\n";
    asionet::test::testDatagramReceiverMaxMessageSize();
    std::cout << "\ntestServiceLargeTransferSize\n\n";
    asionet::test::testServiceLargeTransferSize();
    std::cout << "\ntestNonCopyableMessage\n\n";
    asionet::test::testNonCopyableMessage();
    return 0;
}