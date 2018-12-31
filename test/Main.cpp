//
// Created by philipp on 17.12.17.
//

#include "Test.h"
#include "../include/NetworkingLib/ServiceServer.h"
#include <iostream>

int main(int argc, char ** argv)
{
//    std::cout << "\ntestSyncService\n\n";
//    networking::test::testSyncServices();
//    std::cout << "\ntestAsyncService\n\n";
//    networking::test::testAsyncServices();
//    std::cout <<"\ntestTcpClientTimeout\n\n";
//    networking::test::testTcpClientTimeout();
//    std::cout <<"\ntestMultipleConnections\n\n";
//    networking::test::testMultipleConnections();
//    std::cout << "\ntestStoppingServiceServer\n\n";
//    networking::test::testStoppingServiceServer();
    std::cout << "\ntestAsyncDatagramReceiver\n\n";
    networking::test::testAsyncDatagramReceiver();
//    std::cout << "\ntestPeriodicTimer\n\n";
//    networking::test::testPeriodicTimer();
//    std::cout << "\ntestServiceClientAsyncCallTimeout\n\n";
//    networking::test::testServiceClientAsyncCallTimeout();
    std::cout << "\ntestDatagramSenderAsyncSend\n\n";
    networking::test::testDatagramSenderAsyncSend();
//    std::cout << "\ntestResolver\n\n";
//    networking::test::testResolver();
//    std::cout << "\ntestStringMessageOverDatagram\n\n";
//    networking::test::testStringMessageOverDatagram();
//    std::cout << "\ntestStringMessageOverService\n\n";
//    networking::test::testStringMessageOverService();
//    std::cout << "\ntestServiceServerMaxMessageSize\n\n";
//    networking::test::testServiceServerMaxMessageSize();
//    std::cout << "\ntestServiceClientMaxMessageSize\n\n";
//    networking::test::testServiceClientMaxMessageSize();
//    std::cout << "\ntestDatagramReceiverMaxMessageSize\n\n";
//    networking::test::testDatagramReceiverMaxMessageSize();
//    std::cout << "\ntestServiceLargeTransferSize\n\n";
//    networking::test::testServiceLargeTransferSize();
//    std::cout << "\ntestNonCopyableMessage\n\n";
//    networking::test::testNonCopyableMessage();
    return 0;
}