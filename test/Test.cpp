//
// Created by philipp on 02.12.17.
//

#include "Test.h"
#include "TestUtils.h"
#include <boost/asio/ip/tcp.hpp>
#include <iostream>
#include "../include/ServiceServer.h"
#include "PlatoonService.h"
#include "../include/ServiceClient.h"
#include "../include/DatagramReceiver.h"
#include "../include/DatagramSender.h"

using boost::asio::ip::tcp;

using namespace std::chrono_literals;

namespace asionet
{
namespace test
{

void testSyncServices()
{
    using namespace protocol;
    Networking net1;
    Networking net2;

    auto server = ServiceServer<PlatoonService>::create(net1, 10001);

    server->advertiseService(
        [](const auto & clientEndpoint, const auto & requestMessage, auto & responseMessage)
        {
            std::cout << "Request from "
                      << (int) requestMessage->getVehicleId()
                      << " with type: "
                      << (int) requestMessage->getMessageType()
                      << "\n";
            responseMessage = PlatoonMessage::acceptResponse(1, 42);
        });

    sleep(1);

    int correct = 0;

    auto client = ServiceClient<PlatoonService>::create(net2);
    for (int i = 0; i < 5; i++)
    {
        auto response = client->call(PlatoonMessage::followerRequest(2), "127.0.0.1", 10001, 1s);
        std::cout << "Response from " << (int) response->getVehicleId() << " with type: "
                  << (int) response->getMessageType()
                  << " and platoonId: " << (int) response->getPlatoonId() << "\n";
        if (response->getVehicleId() == 1 && response->getPlatoonId() == 42)
            correct++;
    }

    if (correct == 5)
        std::cout << "SUCCESS!\n";
}

void testAsyncServices()
{
    using namespace protocol;
    Networking net;

    auto server = ServiceServer<PlatoonService>::create(net, 10001);

    server->advertiseService(
        [](const auto & clientEndpoint, const auto & requestMessage, auto & responseMessage)
        {
            std::cout << "Request from "
                      << (int) requestMessage->getVehicleId()
                      << " with type: "
                      << (int) requestMessage->getMessageType()
                      << "\n";
            responseMessage = PlatoonMessage::acceptResponse(1, 42);
        });

    sleep(1);

    std::atomic<std::size_t> pending{5};
    std::atomic<std::size_t> correct{0};

    auto client = ServiceClient<PlatoonService>::create(net);
    for (int i = 0; i < 5; i++)
    {
        client->asyncCall(
            PlatoonMessage::followerRequest(2), "127.0.0.1", 10001, 1s,
            [&pending, &correct](const auto & error, const auto & response)
            {
                if (error)
                    std::cout << "FAILED!\n";
                else
                {
                    std::cout << "Response from " << (int) response->getVehicleId() << " with type: "
                              << (int) response->getMessageType()
                              << " and platoonId: " << (int) response->getPlatoonId() << "\n";
                    if (response->getVehicleId() == 1 && response->getPlatoonId() == 42)
                        correct++;
                }

                pending--;
            });

        while (client->isCalling());
    }

    while (pending > 0);
    if (correct == 5)
        std::cout << "SUCCESS!\n";
}

void testTcpClientTimeout()
{
    using namespace protocol;
    Networking net1;
    Networking net2;

    const auto timeout = 3s;

    auto server = ServiceServer<PlatoonService>::create(net1, 10001);

    server->advertiseService(
        [](const auto & clientEndpoint, const auto & requestMessage, auto & responseMessage)
        {
            // Just sleep for 5 seconds.
            sleep(5);
            responseMessage = PlatoonMessage::acceptResponse(1, 42);
        });

    sleep(1);

    auto client = ServiceClient<PlatoonService>::create(net2);
    auto startTime = boost::posix_time::microsec_clock::local_time();
    try
    {
        auto response = client->call(PlatoonMessage::followerRequest(2), "127.0.0.1", 10001, timeout);
        std::cout << "Response: " << response->getPlatoonId() << "\n";
        std::cout << "FAILED!";
    }
    catch (const error::Aborted & e)
    {
        auto nowTime = boost::posix_time::microsec_clock::local_time();
        auto timeSpend = nowTime - startTime;
        if (timeSpend.seconds() >= 2)
            std::cout << "SUCCESS!\n";
    }
}

void testMultipleConnections()
{
    using namespace protocol;
    Networking net1;
    Networking net2;
    Networking net3;

    auto server1 = ServiceServer<PlatoonService>::create(net1, 10001);
    auto server2 = ServiceServer<PlatoonService>::create(net2, 10002);

    server1->advertiseService(
        [](const auto & clientEndpoint, const auto & requestMessage, auto & responseMessage)
        {
            responseMessage = PlatoonMessage::acceptResponse(1, 42);
        });
    server2->advertiseService(
        [](const auto & clientEndpoint, const auto & requestMessage, auto & responseMessage)
        {
            responseMessage = PlatoonMessage::acceptResponse(2, 43);
        });

    sleep(1);

    auto client = ServiceClient<PlatoonService>::create(net3);

    auto response1 = client->call(PlatoonMessage::followerRequest(1), "127.0.0.1", 10001, 5s);
    std::cout << "Response from " << response1->getVehicleId() << std::endl;

    auto response2 = client->call(PlatoonMessage::followerRequest(2), "127.0.0.1", 10002, 5s);
    std::cout << "Response from " << response2->getVehicleId() << std::endl;

    if (response1->getVehicleId() == 1 && response1->getPlatoonId() == 42 &&
        response2->getVehicleId() == 2 && response2->getPlatoonId() == 43)
        std::cout << "SUCCESS!\n";
}

void testStoppingServiceServer()
{
    using namespace protocol;
    Networking net1;
    Networking net2;

    auto server = ServiceServer<PlatoonService>::create(net1, 10001);

    auto handler = [](const auto & clientEndpoint, const auto & requestMessage, auto & responseMessage)
    {
        // Just sleep for 3 seconds.
        sleep(2);
        responseMessage = PlatoonMessage::acceptResponse(1, 42);
    };

    server->advertiseService(handler);

    sleep(1);

    auto client = ServiceClient<PlatoonService>::create(net2);
    try
    {
        auto response = client->call(PlatoonMessage::followerRequest(42), "127.0.0.1", 10001, 1s);
    }
    catch (const error::Aborted & e)
    {
        server->stop();
        while (server->isBusy());
        server->advertiseService(handler);
        auto response = client->call(PlatoonMessage::followerRequest(42), "127.0.0.1", 10001, 5s);
        if (response->getMessageType() == messageTypes::ACCEPT_RESPONSE)
            std::cout << "SUCCESS!\n";
    }
}

void testAsyncDatagramReceiver()
{
    using namespace protocol;
    Networking net1;
    Networking net2;

    auto receiver = DatagramReceiver<PlatoonMessage>::create(net1, 10000);
    auto sender = DatagramSender<PlatoonMessage>::create(net2);

    std::atomic<bool> running{true};

    receiver->asyncReceive(
        3s,
        [&running](const auto & error, auto & message, const auto & senderHost, auto senderPort)
        {
            if (!error && message->getVehicleId() == 42)
                std::cout << "SUCCESS! Received message from: " << message->getVehicleId() << "\n";
            else
                std::cout << "FAILED!\n";

            running = false;
        });

    sleep(1);

    sender->send(PlatoonMessage::followerRequest(42), "127.0.0.1", 10000, 5s);

    while (running);
}

void testPeriodicTimer()
{
    Networking net;

    auto timer = Timer::create(net);

    int run = 0;
    std::atomic<bool> running{true};
    auto startTime = time::now();
    timer->startPeriodicTimeout(
        1s,
        [&]
        {
            if (run >= 3)
            {
                std::cout << "SUCCESS!\n";
                timer->stop();
                running = false;
                return;
            }

            auto deltaTime = std::chrono::duration_cast<std::chrono::milliseconds>(time::now() - startTime);
            startTime = time::now();
            std::cout << "Delta time [ms]: " << deltaTime.count() << "\n";
            if (std::abs(1s - deltaTime) > 2ms)
                std::cout << "FAILED!\n";
            run++;
        });

    while (running);
}

void testServiceClientAsyncCallTimeout()
{
    using namespace protocol;
    Networking net1;
    Networking net2;

    auto server = ServiceServer<PlatoonService>::create(net1, 10001);

    server->advertiseService(
        [](const auto & clientEndpoint, const auto & requestMessage, auto & responseMessage)
        {
            sleep(3);
            responseMessage = PlatoonMessage::acceptResponse(1, 42);
        });

    sleep(1);

    auto client = ServiceClient<PlatoonService>::create(net2);

    std::atomic<bool> running{true};

    PlatoonMessage response;
    client->asyncCall(
        PlatoonMessage::followerRequest(1), "127.0.0.1", 10001, 1s,
        [&running](const auto & error, const auto & response)
        {
            if (error == error::codes::ABORTED)
                std::cout << "SUCCESS!\n";

            running = false;
        });

    while (running);
}

void testDatagramSenderAsyncSend()
{
    using namespace protocol;
    Networking net;

    auto receiver = DatagramReceiver<PlatoonMessage>::create(net, 10000);
    auto sender = DatagramSender<PlatoonMessage>::create(net);

    std::atomic<bool> running{true};

    receiver->asyncReceive(
        3s,
        [&running](const auto & error, auto & message, const std::string & senderHost, auto senderPort)
        {
            if (error)
            {
                std::cout << "FAILED! (receive error)\n";
                return;
            }

            std::cout << "Sender host: " << senderHost << "\nSender Port: " << senderPort << "\n";
            if (message->getPlatoonId() == 42)
                std::cout << "SUCCESS!\n";

            running = false;
        });

    sender->asyncSend(
        PlatoonMessage::acceptResponse(1, 42), "127.0.0.1", 10000, 1s,
        [](const auto & error)
        {
            if (error)
                std::cout << "FAILED! (send error)\n";
        });

    while (running);
}

void testResolver()
{
    Networking net;

    auto resolver = Resolver::create(net);

    std::atomic<bool> running{true};
    bool thrown{false};

    auto endpoints = resolver->resolve("google.de", "http", 5s);
    for (const auto & endpoint : endpoints)
        std::cout << "ip: " << endpoint.ip << " port: " << endpoint.port << "\n";

    resolver->asyncResolve(
        "google.de", "http", 5s,
        [&running](const auto & error, const auto & endpoints)
        {
            for (const auto & endpoint : endpoints)
                std::cout << "ip: " << endpoint.ip << " port: " << endpoint.port << "\n";

            running = false;
        });

    try
    {
        resolver->resolve("google.de", "http", 5s);
    }
    catch (const error::Busy & error)
    {
        thrown = true;
    }

    while (running);

    if (thrown)
        std::cout << "SUCCESS!\n";
}

void testStringMessageOverDatagram()
{
    Networking net;

    auto receiver = DatagramReceiver<std::string>::create(net, 10000);
    auto sender = DatagramSender<std::string>::create(net);

    std::atomic<bool> running{true};

    std::thread receiverThread{
        [receiver, &running]
        {
            std::string host;
            std::uint16_t port;
            auto message = receiver->receive(host, port, 3s);
            std::cout << "received: host: " << host << " port: " << port << " message: " << message << "\n";
            if (*message == "Hello World!")
                std::cout << "SUCCESS!\n";

            running = false;
        }};

    sleep(1);

    sender->send("Hello World!", "127.0.0.1", 10000, 3s);

    while (running);
    receiverThread.join();
}

void testStringMessageOverService()
{
    Networking net;

    auto server = ServiceServer<StringService>::create(net, 10000);
    auto client = ServiceClient<StringService>::create(net);

    std::atomic<bool> running{true};

    std::atomic<bool> failed{false};

    std::thread receiverThread{
        [server, &running, &failed]
        {
            server->advertiseService(
                [&running, &failed](const auto & endpoint, const auto & request, auto & response)
                {
                    std::cout << "Received request message: " << request << "\n";
                    if (*request != "Ping")
                        failed = true;
                    running = false;
                    response = std::string{"Pong"};
                });
        }};

    sleep(1);

    auto response = client->call(std::string{"Ping"}, "127.0.0.1", 10000, 3s);
    std::cout << "Received response message: " << response << "\n";
    if (*response != "Pong")
        failed = true;

    if (!failed)
        std::cout << "SUCCESS!\n";

    while (running);
    receiverThread.join();
}

void testServiceServerMaxMessageSize()
{
    Networking net;

    std::atomic<bool> running{true};
    std::atomic<bool> syncCallError{false};

    auto server = ServiceServer<StringService>::create(net, 10000, 100);
    server->advertiseService(
        [](auto && ...)
        {
            std::cout << "FAILED! (This should not have been called!\n";
        });

    sleep(1);
    auto client = ServiceClient<StringService>::create(net, 200);
    try
    {
        auto response = client->call(std::string(200, 'a'), "127.0.0.1", 10000, 1s);
    }
    catch (const error::Error &)
    {
        syncCallError = true;
    }

    client->asyncCall(
        std::string(200, 'a'), "127.0.0.1", 10000, 1s,
        [&syncCallError, &running](const auto & error, const auto & message)
        {
            if (error == error::codes::FAILED_OPERATION && syncCallError)
                std::cout << "SUCCESS!\n";

            running = false;
        });

    while (running);
}

void testServiceClientMaxMessageSize()
{
    Networking net;

    std::atomic<bool> running{true};
    std::atomic<std::size_t> serverReceivedCount{0};
    std::atomic<bool> syncCallError{false};

    auto server = ServiceServer<StringService>::create(net, 10000, 200);
    server->advertiseService(
        [&serverReceivedCount](const auto & endpoint, const auto & request, auto & response)
        {
            serverReceivedCount++;
            response = std::string(200, 'a');
        });

    sleep(1);
    auto client = ServiceClient<StringService>::create(net, 100);
    try
    {
        auto response = client->call(std::string{}, "127.0.0.1", 10000, 1s);
    }
    catch (const error::Error &)
    {
        syncCallError = true;
    }

    client->asyncCall(
        std::string(100, 'a'), "127.0.0.1", 10000, 1s,
        [&syncCallError, &running, &serverReceivedCount](const auto & error, const auto & message)
        {
            if (error == error::codes::FAILED_OPERATION && syncCallError && serverReceivedCount == 2)
                std::cout << "SUCCESS!\n";

            running = false;
        });

    while (running);
}

void testServiceLargeTransferSize()
{
    Networking net;

    std::size_t transferSize = 0x10000;
    std::string data(transferSize, 'a');
    std::atomic<bool> running{true};
    std::atomic<bool> success{true};
    auto server = ServiceServer<StringService>::create(net, 10000, transferSize);
    auto client = ServiceClient<StringService>::create(net, transferSize);

    server->advertiseService(
        [&](const auto & endpoint, const auto & request, auto & response)
        {
            if (*request != data)
                success = false;
            response = data;
        });

    client->asyncCall(
        data, "127.0.0.1", 10000, 10s,
        [&](const auto & error, const auto & message)
        {
            if (error || *message != data)
                success = false;
            running = false;
        });

    while (running);
    std::cout << (success ? "SUCCESS!\n" : "FAILED!\n");
}

void testDatagramReceiverMaxMessageSize()
{
    Networking net;

    std::atomic<bool> asyncReceiveFailed{false};
    std::atomic<bool> running{true};

    auto receiver = DatagramReceiver<std::string>::create(net, 10000, 100);
    auto sender = DatagramSender<std::string>::create(net);

    receiver->asyncReceive(
        1s,
        [&asyncReceiveFailed, &running](const auto & error, auto && ... args)
        {
            if (error)
                asyncReceiveFailed = true;
            else
                std::cout << "FAILED! (This should not be called!\n";

            running = false;
        });

    sender->send(std::string(200, 'a'), "127.0.0.1", 10000, 1s);

    while (running);

    running = true;

    std::thread receiveThread{
        [&running, &asyncReceiveFailed, receiver]()
        {
            try
            {
                std::string host;
                std::uint16_t port;
                auto message = receiver->receive(host, port, 3s);
            }
            catch (const error::Error &)
            {
                if (&asyncReceiveFailed)
                    std::cout << "SUCCESS!\n";
            }

            running = false;
        }};

    sleep(1);
    sender->send(std::string(200, 'a'), "127.0.0.1", 10000, 1s);

    while (running);
    receiveThread.join();
}

void testNonCopyableMessage()
{
    // It should only compile to see if the only requirement to Message is to be Default-Constructable.

    try
    {
        Networking net;
        auto sender = DatagramSender<NonCopyableMessage>::create(net);
        auto receiver = DatagramReceiver<NonCopyableMessage>::create(net, 10000);
        sender->send(NonCopyableMessage{}, "127.0.0.1", 10000, 0s);
        std::string host;
        std::uint16_t port;
        auto message = receiver->receive(host, port, 0s);
    }
    catch (...)
    {}

    std::cout << "SUCCESS!\n";
}

}
}

