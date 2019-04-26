# asionet

In case you've ever done some network programming in C++, you probably stumbled upon the quasi-standard boost::asio library.
It uses asynchronous programming making it scalable but on the other side, it takes quite some time to learn how to use it correctly.   
**asionet** is built on top of boost::asio which makes it 100% compatible with it but easier to use at the same time.
For example, managing timeouts and sending and receiving serialized messages is done with only a few lines of code. 

## Prerequisites

In order to use the library, you have to compile with the C++14 standard and make sure to include Boost 1.66 and your system's thread library in your project.
In your CMakeLists.txt, insert:

    set(CMAKE_CXX_STANDARD 14)
    find_package(Boost REQUIRED COMPONENTS system regex)
    find_package(Threads)
    link_libraries(${Boost_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})

## Installation

Get the repository, build and install it.

    $ git clone https://github.com/Badenhoop/asionet
    $ cd asionet
    $ mkdir build
    $ cd build
    $ cmake ..
    $ sudo make install

## Usage

Just insert the following into your CMakeLists.txt file:

    find_package(asionet)
    link_libraries(asionet)

## Tutorial

### Receiving string messages over UDP

The code below listens to port 4242 for receiving a UDP datagram with timeout 1 second.  

```cpp
// Just a typedef of boost::asio::io_context (aka io_service).
asionet::Context context;
// A thread which runs the context object and dispatches asynchronous handlers.
asionet::Worker worker{context};
// UDP datagram receiver operating on port 4242.
asionet::DatagramReceiver<std::string> receiver{context, 4242};
// Receive a string message with timeout 1 second.
receiver.asyncReceive(1s, [](const asionet::error::Error & error, 
                             std::string & message,
                             const boost::asio::ip::udp::endpoint & senderEndpoint) 
{
    if (error) return;
    std::cout << "received: " << message << "\n"
              << "host: " << senderEndpoint.address().to_string() << "\n" 
              << "port: " << senderEndpoint.port() << "\n"; 
});
```

### Sending string messages over UDP

The following code sends a UDP message containing the string "Hello World!" to IP 127.0.0.1 port 4242 with operation timeout 10ms.

```cpp 
asionet::DatagramSender<std::string> sender{context};
sender.asyncSend("Hello World!", "127.0.0.1", 4242, 10ms, [](const asionet::error::Error & error)
{
    if (error)
        // handle error ...
});
```

### Defining custom messages

Wouldn't it be nice to just send your own data types as messages over the network?
Let's assume we want to program the client for an online game so we have to send updates about our player's state.

```cpp 
struct PlayerState
{
    std::string name;
    float posX;
    float posY;
    float health;
};
```

Now we could replace the template parameter from std::string with PlayerState to tell DatagramSender to send PlayerState objects:

```cpp 
asionet::DatagramSender<PlayerState> sender{context};
PlayerState playerState{"WhatAPlayer", 0.15f, 1.7f, 0.1f};
sender.asyncSend(playerState, "127.0.0.1", 4242, 10ms);
```  

The only thing for that to work is to tell asionet how to serialize a PlayerState object into a string of bytes which is simply represented as a string.
Therefore, we could just use the nlohmann json library which is an amazing piece of work by the way.

```cpp
namespace asionet { namespace message {

template<>
struct Encoder<PlayerState>
{
    void operator()(const PlayerState & playerState, std::string & data) const
    {
        auto j = nlohmann::json{ {"name", playerState.name },
                                 {"xPos", playerState.xPos },
                                 {"yPos", playerState.yPos },
                                 {"health", playerState.health } };
        data = j.dump();
    }
};

}}
```

Here we have to create a template specialization of the asionet::message::Encoder<PlayerState> object.
The call operator takes a PlayerState reference as input and expects the data reference to be assigned to the byte string that should be transmitted over the network.

Since we can now send PlayerState objects, we cover the server side next.
Therefore, we have to specialize the asionet::message::Decoder<PlayerState> struct to retrieve the PlayerState object from a buffer object.

```cpp
namespace asionet { namespace message {

template<>
struct Decoder<PlayerState>
{
    template<typename ConstBuffer>
    void operator()(const ConstBuffer & buffer, PlayerState & playerState) const
    {
         std::string s{buffer.begin(), buffer.end()}
         auto j = nlohmann::json::parse(s);
         playerState = PlayerState{
             j.at("name").get<std::string>(),
             j.at("xPos").get<float>(),
             j.at("yPos").get<float>(),
             j.at("health").get<float>()
         };
    }
};

}}
```

Note that we have to define the call operator which takes a template argument and the message to be decoded.
So what exactly is a ConstBuffer?
Since it's a template argument, a ConstBuffer is not an actual class but instead represents an abstract buffer interface.
This interface provides four methods:

- a **size()** function, returning the number of bytes in the buffer
- a **[]-operator** returning a data byte as a char by index
- a **begin()** function to get an iterator to the beginning of the buffer
- a **end()** function to get an iterator to the end of the buffer

By using this abstraction, asionet may internally use the most suitable buffer representation for a specific operation.

Finally, we can set up the UDP receiver as follows:

 ```cpp
asionet::DatagramReceiver<PlayerState> receiver{context, 4242};
receiver.asyncReceive(1s, [](const auto & error, 
                             auto & playerState,
                             auto & senderEndpoint) 
{
    if (error) return;
    std::cout << "player: " << playerState.name << "\n"; 
});
```

### Services

A common network pattern consists of sending a request to a server which reacts by sending a response back to the client.
This happens in http for instance.
Using asionet, it's easy to implement this pattern.

Assume that we want to create a server which delivers chat messages based on a query.
The query consists of two user-IDs defining the chat and the number of most recent messages that should be delivered.
Let's create some classes to model this scenario.

```cpp

struct Query
{
    unsigned long user1;
    unsigned long user2;
    unsigned int numRequestedMessages;
};

struct ChatMessage
{
    unsigned long author;
    std::string content;
};

struct Response
{
    std::vector<ChatMessage> messages;
};

```

Next, we have to specialize the Encoder/Decoder classes for the Query and Response types.
Since this works exactly as shown above using the PlayerState class, we just jump over that.

Now, we have to create a service description:

```cpp
struct ChatService
{
    using RequestMessage = Query;
    using ResponseMessage = Response;
}
```

To create a server which receives incoming requests:

```cpp
asionet::ServiceServer<ChatService> server{context, 4242};
server.advertiseService([](const boost::asio::ip::tcp::endpoint & senderEndpoint, 
                           Query & query,
                           Response & response) 
{
    std::cout << "Requesting " << query->numRequestedMessages << " messages\n";
    response = /* create your response */
});
```
That's it. Simple, right?

Finally, calling the server on the client side looks like this:

```cpp
asionet::ServiceClient<ChatService> client{context};
client.asyncCall(
    Query{10, 12, 50}, "mychatserver.com", 4242, 10s, 
    [](const asionet::error::Error & error, Response & response) 
    {
           if (error) return;
           for (const auto & message : response.messages)
                std::cout << message.author << " wrote: " << message.content << "\n";
    });
```

### Ensuring thread-safety

An important advantage of asynchronous programming is that it is easier to write thread-safe code.
Imagine all asynchronous handlers are invoked from a single thread.
Then there's no need for explicit locking of shared state between the handlers since everything is running in sequence (not concurrently).

However, running only a single thread may not be an option as we want to benefit from being able to run things in parallel.
Therefore, we can wrap handlers inside a **WorkSerializer** object which guarantees that handlers that are wrapped inside the WorkSerializer are executed in sequence.
In fact WorkSerializer just inherits from boost::asio::io_context::strand and can be used in exactly the same manner.

Let's consider this example:

```cpp
asionet::Context context;
// Create 4 threads that are concurrently dispatching handlers from the context object.
asionet::WorkerPool workers{context, 4};
std::size_t counter = 0;
for (std::size_t i = 0; i < 1000000; ++i)
{
    // Post a handler that increments the counter.
    context.post([&] { counter++; });
}
sleep(/* long enough */);
std::cout << counter;
```

If you are familiar with concurrency problems, you are not surprised that the outcome is very likely NOT 1000000.
We can either fix this by making counter atomic or we could employ a WorkSerializer:

```cpp
asionet::Context context;
// Create 4 threads that are concurrently dispatching handlers from the context object.
asionet::WorkerPool workers{context, 4};
asionet::WorkSerializer{context} serializer;
std::size_t counter = 0;
for (std::size_t i = 0; i < 1000000; ++i)
{
    // Post a handler that increments the counter.
    // Now the handler is wrapped by the WorkSerializer.
    context.post(serializer([&] { counter++; }));
    // Alternatively, use:
    // serializer.post([&] { counter++; });
}
sleep(/* long enough */);
std::cout << counter;
```

We just use the WorkSerializer's call operator by taking the handler as input and the output should be 1000000 now. 
So whenever you want your handlers to not run concurrently, just wrap them inside the SAME WorkSerializer object. 

... and of course, in this particular example, there's nothing else that is executed so we could have used only a single worker instead to make it thread-safe.
But imagine you would also have other asynchronous operations running next to those which increment the counter.
Then, all other handlers would still be running concurrently if they are not wrapped inside a WorkSerializer.

And finally, if you have two WorkSerializer objects s1 and s2, they don't care about each other meaning that handlers wrapped inside s1 are running concurrently to handlers wrapped inside s2.

### Lifetime management

We silently ignored the dangerous dangling references problem in the code snippets above which can be easily overlooked.
The problem with running objects in handlers is that by the time a handler is executed, its containing objects could be already destructed.

This is made clear by the following example:

```cpp
asionet::Context context;
asionet::Worker worker{context};

{
    std::string text = "This goes out of scope!";
    context.post([&] { std::cout << text; });
}

// Do something else...
```

Here, 'text' could be already destructed by the time the posted handler executes since this happens on a different thread.
When accessing an invalid reference, the behavior is undefined.
Those types of bugs can be extremely hard to debug.
Therefore, we need some coding practice to systematically avoid this issue.

A good solution to the example above is to use a shared_ptr and pass that inside the lambda capture of the handler.

```cpp
asionet::Context context;
asionet::Worker worker{context};

{
    auto text = std::make_shared<std::string>("I don't mind going out of scope!");
    context.post([text] { std::cout << *text; });
}

// Do something else...
```

However, it can be tedious and of bad performance to make every object a shared_ptr.
Therefore, we could also use the shared_from_this pattern:

```cpp
class ComplexObject : public std::enable_shared_from_this<ComplexObject>
{
public:
    ComplexObject(asionet::Context & context)
        : context(context), sender(context), receier(context, 4242) {}

    void run()
    {
        // Get a shared_ptr of 'this'.
        auto self = shared_from_this();
        // Pass self inside the capture.
        receiver.receive(10s, [self] { /* Safe! */ });
    }

private:
    asionet::DatagramSender<std::string> sender;
    asionet::DatagramReceiver<std::string> receiver;
    // more state ...
};
```

When using ComplexObject, you have to instantiate it in a shared_ptr:

```cpp
auto complexObject = std::make_shared<ComplexObject>(context);
complexObject->run();
```

Even if complexObject leaves its scope, any handlers invoked inside run() which capture the self pointer will not suffer from the dangling references problem.

### Waiting

Sometimes you want to wait for one or more events to complete.
Consider the following:

```cpp
asionet::Context context;
asionet::WorkerPool workers{context, 4};

context.post([] { /* Operation 1 */ });
context.post([] { /* Operation 2 */ });
context.post([] { /* Operation 3 */ });

// Objective: wait until all operation 1, 2 and 3 are done.
```

Of course, we could mess around with atomic booleans or mutexes again but if your program gets more complex, we want something more elegant.
asionet provides the **Waiter** and **Waitable** classes for this purpose:

```cpp
asionet::Context context;
asionet::WorkerPool workers{context, 4};

asionet::Waiter{context} w;
asionet::Waitable w1{w}, w2{w}, w3{w};

context.post(w1([] { /* Operation 1 */ }));
context.post(w2([] { /* Operation 2 */ }));
context.post(w3([] { /* Operation 3 */ }));

w.await(w1 && w2 && w3);
```

Just like the WorkSerializer object, a Waitable wraps its corresponding handler and notifies its Waiter object when the handler finishes execution.
The Waiter object can then await an expression of Waitable objects.
In this case, we want to wait until all Waitable objects are ready which is represented by the chain of &&-operators.
Instead, we could also wait until any handler finishes execution which would be done with:

```cpp
w.await(w1 || w2 || w3);
```

Or we could say that at least two of them should be ready:

```cpp
w.await((w1 && w2) || (w1 && w3) || (w2 && w3));
```

You can also set the state of a Waitable object directly:
```cpp
w1.setReady();
```

If you want to reuse the waitable objects, you have to set their state to waiting again:
```cpp
w.await(w1 && w2 && w3);
// Reset states.
w1.setWaiting();
w2.setWaiting();
w3.setWaiting();
```


### Compatibility with boost::asio

As already mentioned, asionet was designed to be seamlessly usable with existing boost::asio code.
For example, we can send and receive messages with boost::asio::ip::tcp::socket objects directly without having to use the ServiceServer or ServiceClient object:

```cpp
asionet::Context & context;
boost::asio::ip::tcp::socket socket{context};
boost::asio::ip::tcp::endpoint endpoint{
    boost::asio::ip::address::from_string("1.2.3.4"), 4242};
socket.connect(endpoint);
// Send the message over the socket.
asionet::message::asyncSend(socket, PlayerState{"name", 1.f, 0.f, 0.5f}, 1s, [](auto && ...){});
```

