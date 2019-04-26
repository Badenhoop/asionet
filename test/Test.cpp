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
#include "Test.h"
#include "TestUtils.h"
#include <boost/asio/ip/tcp.hpp>
#include <iostream>
#include "../include/asionet/ServiceServer.h"
#include "TestService.h"
#include "../include/asionet/ServiceClient.h"
#include "../include/asionet/DatagramReceiver.h"
#include "../include/asionet/DatagramSender.h"
#include "../include/asionet/Worker.h"
#include "../include/asionet/WorkerPool.h"
#include "../include/asionet/WorkSerializer.h"
#include "../include/asionet/ConstBuffer.h"
#include <gtest/gtest.h>

using boost::asio::ip::tcp;

using namespace std::chrono_literals;
using namespace protocol;

namespace asionet
{
namespace test
{

template<typename Test>
void runTest1(std::size_t numWorkers = 1, std::size_t iterations = 1, asionet::time::Duration pause = 0s)
{
	for (std::size_t i = 0; i < iterations; ++i)
	{
		asionet::Context context;
		WorkerPool workers{context, numWorkers};
		auto test = std::make_shared<Test>(context);
		test->run();
		std::this_thread::sleep_for(pause);
	}
}

template<typename Test>
void runTest2(std::size_t iterations = 1, asionet::time::Duration pause = 0s)
{
	for (std::size_t i = 0; i < iterations; ++i)
	{
	    asionet::Context context1, context2;
	    Worker worker1{context1}, worker2{context2};
	    auto test = std::make_shared<Test>(context1, context2);
	    test->run();
		std::this_thread::sleep_for(pause);
	}
}

struct BasicService : std::enable_shared_from_this<BasicService>
{
    ServiceServer<TestService> server;
    ServiceClient<TestService> client;
    Waiter waiter;

    BasicService(asionet::Context & context)
        : server(context, 10001)
          , client(context)
          , waiter(context)
    {}

    void run()
    {
	    auto self = shared_from_this();
        constexpr std::size_t numCalls{5};
        std::atomic<std::size_t> correct{0};

        server.advertiseService(
            [self](const auto & clientEndpoint, const auto & requestMessage, auto & responseMessage)
            { responseMessage = TestMessage::response(1, 42); });

        for (int i = 0; i < numCalls; i++)
        {
            Waitable waitable{waiter};
            client.asyncCall(
                TestMessage::request(2), "127.0.0.1", 10001, 1s,
                waitable([&, self](const auto & error, auto & response)
                         {
                             EXPECT_FALSE(error);
                             EXPECT_EQ(response.getId(), 1);
                             EXPECT_EQ(response.getValue(), 42);
                             correct++;
                         }));
            waiter.await(waitable);
        }

        EXPECT_EQ(correct, numCalls);
    }
};

TEST(asionetTest, BasicService)
{
    runTest1<BasicService>();
}

struct ClientTimeout : std::enable_shared_from_this<ClientTimeout>
{
    ServiceServer<TestService> server;
    ServiceClient<TestService> client;
    Waiter waiter;

    ClientTimeout(asionet::Context & context1, asionet::Context & context2)
        : server(context1, 10001)
          , client(context2)
          , waiter(context2)
    {}

    void run()
    {
        auto self = shared_from_this();

        const auto timeout = 10ms;
	    const auto serviceDuration = 15ms;

        server.advertiseService(
            [&, self](const auto & clientEndpoint, const auto & requestMessage, auto & responseMessage)
            {
                std::this_thread::sleep_for(serviceDuration);
                responseMessage = TestMessage::response(1, 42);
            });

        Waitable waitable{waiter};
        auto startTime = asionet::time::now();
        client.asyncCall(TestMessage::request(2), "127.0.0.1", 10001, timeout,
                         waitable([&, self](const auto & error, const auto & response)
                                  {
                                      auto nowTime = asionet::time::now();
                                      auto timeSpend = nowTime - startTime;
                                      auto delta = std::abs(timeSpend - timeout);
	                                  EXPECT_EQ(error, error::aborted);
                                      EXPECT_LE(delta, 2ms);
                                  }));
        waiter.await(waitable);
    }
};

TEST(asionetTest, ClientTimeout)
{
    runTest2<ClientTimeout>();
}

struct MultipleCalls : std::enable_shared_from_this<MultipleCalls>
{
	ServiceServer<TestService> server1;
	ServiceServer<TestService> server2;
	ServiceClient<TestService> client;
	TestMessage response1{}, response2{};
	Waiter waiter;
	Waitable waitable1{waiter}, waitable2{waiter};

    MultipleCalls(Context & context)
        : server1(context, 10001)
        , server2(context, 10002)
        , client(context)
        , waiter(context)
    {}

    void run()
    {
        auto self = shared_from_this();
        server1.advertiseService(
            [self](const auto & clientEndpoint, const auto & requestMessage, auto & responseMessage)
            { responseMessage = TestMessage::response(1, 42); });
        server2.advertiseService(
            [self](const auto & clientEndpoint, const auto & requestMessage, auto & responseMessage)
            { responseMessage = TestMessage::response(2, 43); });

        client.asyncCall(TestMessage::request(1), "127.0.0.1", 10001, 5s,
                         waitable1([&, self] (const auto & error, const auto & response) { response1 = response; }));

        client.asyncCall(TestMessage::request(2), "127.0.0.1", 10002, 5s,
                         waitable2([&, self] (const auto & error, const auto & response) { response2 = response; }));

        waiter.await(waitable1 && waitable2);

        EXPECT_EQ(response1.getId(), 1);
        EXPECT_EQ(response2.getId(), 2);
        EXPECT_EQ(response1.getValue(), 42);
        EXPECT_EQ(response2.getValue(), 43);
    }
};

TEST(asionetTest, MultipleCalls)
{
	runTest1<MultipleCalls>();
}

struct CancelingServer : std::enable_shared_from_this<CancelingServer>
{
    ServiceServer<TestService> server;
    ServiceClient<TestService> client;
    Waiter waiter;

    CancelingServer(Context & context)
        : server(context, 10001)
          , client(context)
          , waiter(context)
    {}

    void run()
    {
        auto self = shared_from_this();

        auto handler = [self](const auto & clientEndpoint, const auto & requestMessage, auto & responseMessage)
        { responseMessage = TestMessage::response(requestMessage.getId(), 1); };

        server.advertiseService(handler);

        TestMessage response{};
        Waitable waitable{waiter};
        auto callHandler = waitable([&, self](const auto & error, const auto & resp) { response = resp; });

        client.asyncCall(TestMessage::request(42), "127.0.0.1", 10001, 1s, callHandler);
        waiter.await(waitable);
        waitable.setWaiting();

	    server.cancel();
	    std::this_thread::sleep_for(10ms);  // give os some time to release the port
        server.advertiseService(handler);

        client.asyncCall(TestMessage::request(43), "127.0.0.1", 10001, 1s, callHandler);
        waiter.await(waitable);

        EXPECT_EQ(response.getId(), 43);
        EXPECT_EQ(response.getMessageType(), messageTypes::RESPONSE);
    }
};

TEST(asionetTest, CancelingServer)
{
    runTest1<CancelingServer>();
}

struct BasicDatagram : std::enable_shared_from_this<BasicDatagram>
{
    DatagramReceiver<TestMessage> receiver;
    DatagramSender<TestMessage> sender;
    Waiter waiter;

    BasicDatagram(asionet::Context & context)
        : receiver(context, 10000)
        , sender(context)
        , waiter(context)
    {}

    void run()
    {
        auto self = shared_from_this();
        Waitable waitable{waiter};
        receiver.asyncReceive(
            1s,
            waitable([self](const auto & error, auto & message, const auto & senderEndpoint)
                     {
                         EXPECT_FALSE(error);
                         EXPECT_EQ(message.getId(), 42);
                     }));
        sender.asyncSend(TestMessage::request(42), "127.0.0.1", 10000, 1s, [self](const auto & error) { EXPECT_FALSE(error); });
        waiter.await(waitable);
    }
};

TEST(asionetTest, BasicDatagram)
{
    runTest1<BasicDatagram>();
}

struct PeriodicTimeout : std::enable_shared_from_this<PeriodicTimeout>
{
    Timer timer;
    Waiter waiter;

    PeriodicTimeout(asionet::Context & context)
        : timer(context)
          , waiter(context)
    {}

    void run()
    {
        auto self = shared_from_this();
        std::atomic<std::size_t> run{0};
        std::size_t runs = 5;
        auto startTime = time::now();
        auto period = 10ms;
        Waitable waitable{waiter};
        timer.startPeriodicTimeout(
            period,
            [&, self]
            {
                if (run >= runs)
                {
	                timer.cancel();
                    waitable.setReady();
                    return;
                }

                auto periodTime = time::now() - startTime;
                startTime = time::now();
                auto delta = std::abs(periodTime - period);
	            EXPECT_LE(delta, 2ms);
                run++;
            });
        waiter.await(waitable);
    }
};

TEST(asionetTest, PeriodicTimeout)
{
    runTest1<PeriodicTimeout>();
}

struct QueuedDatagramSending : std::enable_shared_from_this<QueuedDatagramSending>
{
	DatagramReceiver<TestMessage> receiver;
	DatagramSender<TestMessage> sender;
	Waiter waiter;

	QueuedDatagramSending(asionet::Context & context)
		: receiver(context, 10000)
		 , sender(context)
		 , waiter(context)
	{}

	void run()
	{
		auto self = shared_from_this();
		std::atomic<bool> running{true};
		std::atomic<std::size_t> receivedMessages{0};
		constexpr std::size_t sentMessages{10};
		Waitable waitable{waiter};

		DatagramReceiver<TestMessage>::ReceiveHandler receiveHandler =
			[&, self](const auto & error, auto & message, const auto & senderEndpoint)
			{
				EXPECT_FALSE(error);
				auto i = message.getValue();
				EXPECT_EQ(i, receivedMessages); // correct order
				receivedMessages++;
				if (receivedMessages == sentMessages)
				{
					waitable.setReady();
					return;
				}
				receiver.asyncReceive(1s, receiveHandler);
			};

		receiver.asyncReceive(1s, receiveHandler);

		for (std::size_t i = 0; i < sentMessages; ++i)
		{
			sender.asyncSend(
				TestMessage::response(1, i), "127.0.0.1", 10000, 1s,
				[self](const auto & error) { EXPECT_FALSE(error); });
		}

		waiter.await(waitable);
		EXPECT_EQ(receivedMessages, sentMessages);
	}
};

TEST(asionetTest, QueuedDatagramSending)
{
	runTest1<QueuedDatagramSending>();
}

struct Resolving : std::enable_shared_from_this<Resolving>
{
	Resolver<boost::asio::ip::tcp> resolver;
	Waiter waiter;

	Resolving(asionet::Context & context)
		: resolver(context)
		  , waiter(context)
	{}

	void run()
	{
		auto self = shared_from_this();
		Waitable waitable{waiter};
		resolver.asyncResolve(
			"google.de", "http", 5s,
			waitable([self](const auto & error, const auto & endpointIterator) { EXPECT_FALSE(error); }));
		waiter.await(waitable);
	}
};

TEST(asionetTest, Resolving)
{
	runTest1<Resolving>();
}

struct StringDatagram : std::enable_shared_from_this<StringDatagram>
{
	DatagramReceiver<std::string> receiver;
	DatagramSender<std::string> sender;
	Waiter waiter;

	StringDatagram(asionet::Context & context)
		: receiver(context, 10000)
		  , sender(context)
		  , waiter(context)
	{}

	void run()
	{
		auto self = shared_from_this();
		Waitable waitable{waiter};
		receiver.asyncReceive(
			1s, waitable([&, self](const auto & error, auto & message, const auto & senderEndpoint)
			             {
				             EXPECT_FALSE(error);
				             EXPECT_EQ(message, "Hello World!");
			             }));
		sender.asyncSend("Hello World!", "127.0.0.1", 10000, 1s, [self] (const auto & error) { EXPECT_FALSE(error); });
		waiter.await(waitable);
	}
};

TEST(asionetTest, StringDatagram)
{
	runTest1<StringDatagram>();
}

struct StringOverService : std::enable_shared_from_this<StringOverService>
{
	ServiceServer<StringService> server;
	ServiceClient<StringService> client;
	Waiter waiter;

	StringOverService(Context & context)
		: server(context, 10000)
		  , client(context)
		  , waiter(context)
	{}

	void run()
	{
		auto self = shared_from_this();
		Waitable waitable{waiter};
		server.advertiseService(
			[&, self](const auto & endpoint, const auto & request, auto & response)
			{
				EXPECT_EQ(request, "Ping");
				response = std::string{"Pong"};
			});
		client.asyncCall("Ping", "127.0.0.1", 10000, 1s,
		                 waitable([&, self](const auto & error, const auto & resp)
		                          {
			                          EXPECT_FALSE(error);
			                          EXPECT_EQ(resp, "Pong");
		                          }));
		waiter.await(waitable);
	}
};

TEST(asionetTest, StringOverService)
{
	runTest1<StringOverService>();
}

struct MaxMessageSizeServer : std::enable_shared_from_this<MaxMessageSizeServer>
{
	ServiceServer<StringService> server;
	ServiceClient<StringService> client;
	Waiter waiter;

	MaxMessageSizeServer(Context & context)
		: server(context, 10000, 100)
		  , client(context, 200)
		  , waiter(context)
	{}

	void run()
	{
		auto self = shared_from_this();
		server.advertiseService(
			[self](auto && ...)
			{
				FAIL();
			});
		Waitable waitable{waiter};
		client.asyncCall(
			std::string(200, 'a'), "127.0.0.1", 10000, 1s,
			waitable([&, self](const auto & error, const auto & message)
			         {
				         EXPECT_EQ(error, error::failedOperation);
			         }));
		waiter.await(waitable);
	}
};

TEST(asionetTest, MaxMessageSizeServer)
{
	runTest1<MaxMessageSizeServer>();
}

struct MaxMessageSizeClient : std::enable_shared_from_this<MaxMessageSizeClient>
{
	ServiceServer<StringService> server;
	ServiceClient<StringService> client;
	Waiter waiter;

	MaxMessageSizeClient(Context & context)
		: server(context, 10000, 200)
		  , client(context, 100)
		  , waiter(context)
	{}

	void run()
	{
		auto self = shared_from_this();
		server.advertiseService(
			[&, self](const auto & endpoint, const auto & request, auto & response)
			{
				response = std::string(200, 'a');
			});
		Waitable waitable{waiter};
		client.asyncCall(
			std::string(100, 'a'), "127.0.0.1", 10000, 1s,
			waitable([&, self](const auto & error, const auto & message)
			         {
				         EXPECT_EQ(error, error::invalidFrame);
			         }));
		waiter.await(waitable);
	}
};

TEST(asionetTest, MaxMessageSizeClient)
{
	runTest1<MaxMessageSizeClient>();
}

struct MaxMessageSizeDatagramReceiver : std::enable_shared_from_this<MaxMessageSizeDatagramReceiver>
{
	DatagramReceiver<std::string> receiver;
	DatagramSender<std::string> sender;
	Waiter waiter;

	MaxMessageSizeDatagramReceiver(asionet::Context & context)
		: receiver(context, 10000, 100)
		  , sender(context)
		  , waiter(context)
	{}

	void run()
	{
		auto self = shared_from_this();
		Waitable waitable{waiter};
		receiver.asyncReceive(1s, waitable([&, self](const auto & error, auto && ... args)
		                                   { EXPECT_EQ(error, error::invalidFrame); }));
		sender.asyncSend(std::string(200, 'a'), "127.0.0.1", 10000, 1s,
		                 [self] (const auto & error) { EXPECT_FALSE(error); });
		waiter.await(waitable);
	}
};

TEST(asionetTest, MaxMessageSizeDatagramReceiver)
{
	runTest1<MaxMessageSizeDatagramReceiver>();
}

struct LargeTransferSize : std::enable_shared_from_this<LargeTransferSize>
{
	ServiceServer<StringService> server;
	ServiceClient<StringService> client;
	Waiter waiter;
	static constexpr std::size_t transferSize = 0x10000;

	LargeTransferSize(Context & context)
		: server(context, 10000, transferSize)
		  , client(context, transferSize)
		  , waiter(context)
	{}

	void run()
	{
		auto self = shared_from_this();
		std::string data(transferSize, 'a');
		server.advertiseService(
			[&, self](const auto & endpoint, const auto & request, auto & response)
			{
				EXPECT_EQ(request, data);
				response = data;
			});
		Waitable waitable{waiter};
		client.asyncCall(
			data, "127.0.0.1", 10000, 10s,
			waitable([&, self](const auto & error, const auto & response)
			         {
				         EXPECT_FALSE(error);
				         EXPECT_EQ(response, data);
			         }));
		waiter.await(waitable);
	}
};

TEST(asionetTest, LargeTransferSize)
{
	runTest1<LargeTransferSize>();
}

// --- ATTENTION ---
// The following tests must be checked manually.

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
}

TEST(asionetTest, ConstStreamBuffer)
{
	boost::asio::streambuf streambuf;
	std::ostream os{&streambuf};
	std::string s{"Hello World!"};
	os << '1' << '2' << '3' << '4';
	os << s;
	std::size_t n = s.size();
	std::size_t offset = 4;
	internal::ConstStreamBuffer buffer{streambuf, n, offset};
	EXPECT_EQ(buffer.size(), n);
	EXPECT_EQ(buffer[4], 'o');
	std::string o{buffer.begin(), buffer.end()};
	EXPECT_EQ(o, s);
}

TEST(asionetTest, ConstVectorBuffer)
{
	std::vector<char> v = {'1', '2', '3', '4', 'A', 'B', 'C'};
	internal::ConstVectorBuffer buffer{v, 3, 4};
	EXPECT_EQ(buffer.size(), 3);
	EXPECT_EQ(buffer[2], 'C');
	std::string o{buffer.begin(), buffer.end()};
	EXPECT_EQ(o, std::string{"ABC"});
}

}
}