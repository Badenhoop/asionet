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
#include "../include/Worker.h"
#include "../include/WorkerPool.h"
#include "../include/WorkSerializer.h"

using boost::asio::ip::tcp;

using namespace std::chrono_literals;

namespace asionet
{
namespace test
{

void testAsyncServices()
{
    using namespace protocol;
    Context context;
    Worker worker{context};

    ServiceServer<PlatoonService> server{context, 10001};
    server.advertiseService(
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

    std::size_t count = 5;
    std::atomic<std::size_t> pending{count};
    std::atomic<std::size_t> correct{0};
    std::atomic<bool> calling{false};

    ServiceClient<PlatoonService> client{context};
    Waiter waiter{context};

    for (int i = 0; i < count; i++)
    {
        Waitable waitable{waiter};
        client.asyncCall(
            PlatoonMessage::followerRequest(2), "127.0.0.1", 10001, 1s,
            waitable([&](const auto & error, const auto & response)
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
            }));
        waiter.await(waitable);
    }

    while (pending > 0);
    if (correct == count)
        std::cout << "SUCCESS!\n";
}

void testTcpClientTimeout()
{
    using namespace protocol;
    Context context1, context2;
    Worker worker1{context1};
    Worker worker2{context2};

    const auto timeout = 3s;

    ServiceServer<PlatoonService> server{context1, 10001};
    server.advertiseService(
        [](const auto & clientEndpoint, const auto & requestMessage, auto & responseMessage)
        {
            // Just sleep for 5 seconds.
            sleep(4);
            responseMessage = PlatoonMessage::acceptResponse(1, 42);
        });

    sleep(1);

    ServiceClient<PlatoonService> client{context2};
    Waiter waiter{context2};
    Waitable waitable{waiter};

    auto startTime = boost::posix_time::microsec_clock::local_time();

    client.asyncCall(PlatoonMessage::followerRequest(2), "127.0.0.1", 10001, timeout,
    waitable([&](const auto & error, const auto & response)
    {
        auto nowTime = boost::posix_time::microsec_clock::local_time();
        auto timeSpend = nowTime - startTime;
        if (error == error::codes::ABORTED && timeSpend.seconds() >= 2)
        {
            std::cout << "SUCCESS!\n";
            return;
        }
        std::cout << "Response: " << response->getPlatoonId() << "\n";
        std::cout << "FAILED!";
    }));

    waiter.await(waitable);
}

void testMultipleConnections()
{
    using namespace protocol;
    Context context;
    Worker worker{context};

    ServiceServer<PlatoonService> server1{context, 10001};
    ServiceServer<PlatoonService> server2{context, 10002};

    server1.advertiseService(
        [](const auto & clientEndpoint, const auto & requestMessage, auto & responseMessage)
        { responseMessage = PlatoonMessage::acceptResponse(1, 42); });
    server2.advertiseService(
        [](const auto & clientEndpoint, const auto & requestMessage, auto & responseMessage)
        { responseMessage = PlatoonMessage::acceptResponse(2, 43); });

    sleep(1);

    ServiceClient<PlatoonService> client(context);
    PlatoonMessage response1{}, response2{};
    Waiter waiter{context};
    Waitable waitable1{waiter}, waitable2{waiter};

    client.asyncCall(PlatoonMessage::followerRequest(1), "127.0.0.1", 10001, 5s,
        waitable1([&] (auto error, const auto & response) { response1 = *response; }));

    client.asyncCall(PlatoonMessage::followerRequest(2), "127.0.0.1", 10002, 5s,
        waitable2([&] (auto error, const auto & response) { response2 = *response; }));

    waiter.await(waitable1 && waitable2);

    if (response1.getVehicleId() == 1 && response1.getPlatoonId() == 42 &&
        response2.getVehicleId() == 2 && response2.getPlatoonId() == 43)
        std::cout << "SUCCESS!\n";
}

void testStoppingServiceServer()
{
    using namespace protocol;
    Context context;
    Worker worker{context};

    ServiceServer<PlatoonService> server{context, 10001};

    auto handler = [](const auto & clientEndpoint, const auto & requestMessage, auto & responseMessage)
    { responseMessage = PlatoonMessage::acceptResponse(requestMessage->getVehicleId(), 1); };

    server.advertiseService(handler);

    sleep(1);

    ServiceClient<PlatoonService> client{context};
    PlatoonMessage response{};
	Waiter waiter{context};
	Waitable waitable{waiter};
	auto callHandler = waitable([&](auto error, const auto & resp) { response = *resp; });

	client.asyncCall(PlatoonMessage::followerRequest(42), "127.0.0.1", 10001, 1s, callHandler);
	waiter.await(waitable);
	waitable.setWaiting();

	server.stop();
    sleep(1);
    server.advertiseService(handler);

    client.asyncCall(PlatoonMessage::followerRequest(43), "127.0.0.1", 10001, 1s, callHandler);
	waiter.await(waitable);
    if (response.getVehicleId() == 43 && response.getMessageType() == messageTypes::ACCEPT_RESPONSE)
	    std::cout << "SUCCESS!\n";
    else
	    std::cout << "FAILURE!\n";
}

void testAsyncDatagramReceiver()
{
    using namespace protocol;
    Context context;
    Worker worker{context};

    DatagramReceiver<PlatoonMessage> receiver{context, 10000};
    DatagramSender<PlatoonMessage> sender{context};

	Waiter waiter{context};
	Waitable waitable{waiter};

    receiver.asyncReceive(
        3s,
        waitable([](const auto & error, auto & message, const auto & senderHost, auto senderPort)
        {
            if (!error && message->getVehicleId() == 42)
                std::cout << "SUCCESS! Received message from: " << message->getVehicleId() << "\n";
            else
                std::cout << "FAILED!\n";
        }));

    sleep(1);

    sender.asyncSend(PlatoonMessage::followerRequest(42), "127.0.0.1", 10000, 5s, [](auto && ...) {});
	waiter.await(waitable);
}

void testPeriodicTimer()
{
    Context context;
    Worker worker{context};

    Timer timer{context};
    Waiter waiter{context};
    Waitable waitable{waiter};

    int run = 0;
    auto startTime = time::now();
    timer.startPeriodicTimeout(
        1s,
        [&]
        {
            if (run >= 3)
            {
                std::cout << "SUCCESS!\n";
                timer.stop();
	            waitable.setReady();
                return;
            }

            auto deltaTime = std::chrono::duration_cast<std::chrono::milliseconds>(time::now() - startTime);
            startTime = time::now();
            std::cout << "Delta time [ms]: " << deltaTime.count() << "\n";
            if (std::abs(1s - deltaTime) > 2ms)
            {
	            std::cout << "FAILED!\n";
	            waitable.setReady();
            }
            run++;
        });

    waiter.await(waitable);
}

void testServiceClientAsyncCallTimeout()
{
    using namespace protocol;
    // two contexts because event loop blocking with sleep -> can't simulate timeout
    Context context1, context2;
	Worker worker1{context1}, worker2{context2};

    ServiceServer<PlatoonService> server{context1, 10001};

    server.advertiseService(
        [](const auto & clientEndpoint, const auto & requestMessage, auto & responseMessage)
        {
            sleep(3);
            responseMessage = PlatoonMessage::acceptResponse(1, 42);
        });

    sleep(1);

    ServiceClient<PlatoonService> client{context2};
    Waiter waiter{context2};
    Waitable waitable{waiter};

	PlatoonMessage response;
	client.asyncCall(
		PlatoonMessage::followerRequest(1), "127.0.0.1", 10001, 1s,
		waitable([](const auto & error, const auto & response)
		         {
			         if (error == error::codes::ABORTED)
				         std::cout << "SUCCESS!\n";
		         }));

	waiter.await(waitable);
}

void testDatagramSenderAsyncSend()
{
    using namespace protocol;
    Context context;
    Worker worker{context};

    DatagramReceiver<PlatoonMessage> receiver{context, 10000};
    DatagramSender<PlatoonMessage> sender{context};

    Waiter waiter{context};
    Waitable waitable{waiter};

    receiver.asyncReceive(
        3s,
        waitable([](const auto & error, auto & message, const std::string & senderHost, auto senderPort)
        {
            if (error)
            {
                std::cout << "FAILED! (receive error)\n";
                return;
            }

            std::cout << "Sender host: " << senderHost << "\nSender Port: " << senderPort << "\n";
            if (message->getPlatoonId() == 42)
                std::cout << "SUCCESS!\n";
        }));

    sender.asyncSend(
		PlatoonMessage::acceptResponse(1, 42), "127.0.0.1", 10000, 1s,
		[](const auto & error)
        {
            if (error)
                std::cout << "FAILED! (send error)\n";
        });

    waiter.await(waitable);
}

void testDatagramSenderQueuedSending()
{
    using namespace protocol;
    Context context;
    Worker worker{context};

    DatagramReceiver<PlatoonMessage> receiver(context, 10000);
    DatagramSender<PlatoonMessage> sender(context);

    std::atomic<bool> running{true};
    std::atomic<std::size_t> receivedMessages{0};
    constexpr std::size_t sentMessages{10};

    DatagramReceiver<PlatoonMessage>::ReceiveHandler receiveHandler = [&](const auto & error, auto & message, const std::string & senderHost, auto senderPort)
    {
        if (error)
        {
            std::cout << "FAILED! (receive error)\n";
            running = false;
            return;
        }

        auto i = message->getPlatoonId();

        if (i != receivedMessages)
        {
            std::cout << "FAILED! (incorrect order)\n";
            running = false;
            return;
        }

        std::cout << "Received message #" << i << "\n";

        receivedMessages++;
        if (receivedMessages == sentMessages)
        {
            std::cout << "SUCCESS\n";
            running = false;
            return;
        }

        receiver.asyncReceive(1s, receiveHandler);
    };

    receiver.asyncReceive(1s, receiveHandler);

    for (std::size_t i = 0; i < sentMessages; ++i)
    {
	    sender.asyncSend(
		    PlatoonMessage::acceptResponse(1, i), "127.0.0.1", 10000, 1s,
            [](const auto & error)
            {
                if (error)
                    std::cout << "FAILED! (send error)\n";
            });
    }

    while (running);
}

void testResolver()
{
    Context context;
    Worker worker{context};
    Resolver resolver{context};
    Waiter waiter{context};
    Waitable waitable{waiter};

    resolver.asyncResolve(
        "google.de", "http", 5s,
        waitable([](const auto & error, const auto & endpoints)
        {
            if (error)
            {
                std::cout << "FAILURE!\n";
                return;
            }

            for (const auto & endpoint : endpoints)
                std::cout << "ip: " << endpoint.ip << " port: " << endpoint.port << "\n";

            std::cout << "SUCCESS!\n";
        }));

    waiter.await(waitable);
}

void testStringMessageOverDatagram()
{
    Context context;
    Worker worker{context};

    DatagramReceiver<std::string> receiver{context, 10000};
    DatagramSender<std::string> sender{context};

    Waiter waiter{context};
    Waitable waitable{waiter};

    receiver.asyncReceive(
        3s, waitable([&](const auto & error, auto & message, const std::string & senderHost, auto senderPort)
        {
            std::cout << "received: host: " << senderHost << " port: " << senderPort << " message: " << *message << "\n";
            if (*message == "Hello World!")
                std::cout << "SUCCESS!\n";
        }));

    sleep(1);

    sender.asyncSend("Hello World!", "127.0.0.1", 10000, 3s);
    waiter.await(waitable);
}

void testStringMessageOverService()
{
    Context context;
    Worker worker{context};

    ServiceServer<StringService> server{context, 10000};
    ServiceClient<StringService> client{context};

    Waiter waiter{context};
    Waitable waitable{waiter};

    std::atomic<bool> failed{false};

    server.advertiseService(
        [&](const auto & endpoint, const auto & request, auto & response)
        {
            if (*request != "Ping")
                failed = true;
            response = std::string{"Pong"};
        });

    sleep(1);

    std::string response;

    client.asyncCall("Ping", "127.0.0.1", 10000, 3s,
                      waitable([&](auto error, const auto & resp) { response = *resp; }));

    waiter.await(waitable);

    if (response != "Pong")
        failed = true;

    if (!failed)
        std::cout << "SUCCESS!\n";
}

void testServiceServerMaxMessageSize()
{
    Context context;
    Worker worker{context};

    ServiceServer<StringService> server{context, 10000, 100};
    server.advertiseService(
        [](auto && ...)
        {
            std::cout << "FAILED! (This should not have been called!\n";
        });

    sleep(1);

    ServiceClient<StringService> client{context, 200};
	Waiter waiter{context};
	Waitable waitable{waiter};

    client.asyncCall(
        std::string(200, 'a'), "127.0.0.1", 10000, 1s,
        waitable([&](const auto & error, const auto & message)
        {
            if (error == error::codes::FAILED_OPERATION)
                std::cout << "SUCCESS!\n";
        }));

    waiter.await(waitable);
}

void testServiceClientMaxMessageSize()
{
    Context context;
    Worker worker{context};

    ServiceServer<StringService> server{context, 10000, 200};
    server.advertiseService(
        [&](const auto & endpoint, const auto & request, auto & response)
        {
            response = std::string(200, 'a');
        });

    sleep(1);

    ServiceClient<StringService> client{context, 100};
    Waiter waiter{context};
    Waitable waitable{waiter};

    client.asyncCall(
        std::string(100, 'a'), "127.0.0.1", 10000, 1s,
        waitable([&](const auto & error, const auto & message)
        {
            if (error == error::codes::FAILED_OPERATION)
                std::cout << "SUCCESS!\n";
        }));

    waiter.await(waitable);
}

void testDatagramReceiverMaxMessageSize()
{
	Context context;
	Worker worker{context};

	DatagramReceiver<std::string> receiver{context, 10000, 100};
	DatagramSender<std::string> sender{context};

	Waiter waiter{context};
	Waitable waitable{waiter};

	receiver.asyncReceive(
		1s,
		waitable([&](const auto & error, auto && ... args)
		{
			if (error == error::codes::FAILED_OPERATION)
				std::cout << "SUCCESS!\n";
			else
				std::cout << "FAILED! (This should not be called!\n";
		}));

	sender.asyncSend(std::string(200, 'a'), "127.0.0.1", 10000, 1s);

	waiter.await(waitable);
}

void testServiceLargeTransferSize()
{
    Context context;
    Worker worker{context};

    std::size_t transferSize = 0x10000;
    std::string data(transferSize, 'a');
    std::atomic<bool> success{true};
    ServiceServer<StringService> server{context, 10000, transferSize};
    ServiceClient<StringService> client{context, transferSize};

    server.advertiseService(
        [&](const auto & endpoint, const auto & request, auto & response)
        {
            if (*request != data)
                success = false;
            response = data;
        });

    sleep(1);
    Waiter waiter{context};
    Waitable waitable{waiter};

    client.asyncCall(
        data, "127.0.0.1", 10000, 10s,
        waitable([&](const auto & error, const auto & message)
        {
            if (error || *message != data)
                success = false;
        }));

    waiter.await(waitable);
    std::cout << (success ? "SUCCESS!\n" : "FAILED!\n");
}

void testNonCopyableMessage()
{
    // It should only compile to see if the only requirement to Message is to be Default-Constructable.

    Context context;
    Worker worker{context};
    DatagramReceiver<NonCopyableMessage> receiver{context, 10000};
    DatagramSender<NonCopyableMessage> sender{context};
    sender.asyncSend(NonCopyableMessage{}, "127.0.0.1", 10000, 0s);
    std::string host;
    std::uint16_t port;
    receiver.asyncReceive(0s, [](auto && ...) {});
    std::cout << "SUCCESS!\n";
}

void testWorkerPool()
{
    Context context;
    WorkerPool pool{context, 2};
    std::mutex mutex;
    using namespace std::chrono_literals;
    for (std::size_t i = 0; i < 50; i++)
    {
        context.post(
            [i, &mutex]
            {
                std::lock_guard<std::mutex> lock{mutex};
                std::cout << "output: " << i << " from: " << std::this_thread::get_id() << "\n";
                std::this_thread::sleep_for(1ms);
            });
    }

    sleep(1);
}

void testWorkSerializer()
{
    Context context;
    WorkerPool pool{context, 2};
    WorkSerializer serializer{context};
    using namespace std::chrono_literals;
    for (std::size_t i = 0; i < 50; i++)
    {
        context.post(serializer(
            [i, &serializer]
            {
                std::cout << "output: " << i << " from: " << std::this_thread::get_id() << std::endl;
                std::this_thread::sleep_for(1ms);
            }));
    }

    sleep(1);
}

}
}


