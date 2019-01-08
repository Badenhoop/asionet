# asionet

In case you've ever done some network programming in C++, you probably stumbled upon the quasi-standard boost::asio library.
It uses asynchronous programming making it scalable but on the other side, it takes quite some time to learn how to use it correctly.   
**asionet** is built on top of boost::asio which makes it 100% compatible with it but easier to use at the same time.
For example, managing timeouts and sending and receiving serialized messages is done with only a few lines of code. 

## Installation

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

## Examples

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
receiver.asyncReceive(1s, [](const asionet::Error & error, 
                             const std::shared_ptr<std::string> & message,
                             const std::string & host, 
                             std::uint16_t port) {
    if (error) return;
    std::cout << "received: " << *message
              << "\nfrom: " << host << "@" << port << "\n"; 
});
```

### Sending UDP string messages over UDP

The following code sends a UDP message containing the string "Hello World!" to IP 127.0.0.1 port 4242 with operation timeout 10ms.

```cpp 
asionet::DatagramSender<std::string> sender{context};
sender.asyncSend("Hello World!", "127.0.0.1", 4242, 10ms);
```

### Defining custom messages

Wouldn't it be nice to just send our own data types as messages over the network?
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

Now we could replace the template parameter from std::string into PlayerState to tell DatagramSender to send PlayerState objects:

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
}

}}
```

Here we have to create a template specialization of the asionet::message::Encoder<PlayerState> object.
The call operator takes a PlayerState reference as input and expects the data reference to be assigned to the byte string that should be transmitted over the network.

Since we can now send PlayerState objects, we cover the server side next.
Therefore, we have to specialize the asionet::message::Decoder<PlayerState> struct to retrieve the PlayerState object from a received string of bytes.

```cpp
namespace asionet { namespace message {

template<>
struct Decoder<PlayerState>
{
    std::shared_ptr<PlayerState> operator()(const std::string & data) const
    {
         auto j = nlohmann::json::parse(data);
         return std::make_shared<PlayerState>(
             j.at("name").get<std::string>(),
             j.at("xPos").get<float>(),
             j.at("yPos").get<float>(),
             j.at("health").get<float>()
         );
    }
}

}}
```

Note that we have to return a std::shared_ptr<PlayerState> in this case.
Finally, we can set up the UDP receiver as follows:

 ```cpp
asionet::DatagramReceiver<PlayerState> receiver{context, 4242};
receiver.asyncReceive(1s, [](const asionet::Error & error, 
                             const std::shared_ptr<PlayerState> & playerState,
                             const std::string & host, 
                             std::uint16_t port) {
    if (error) return;
    std::cout << "player: " << playerState->name << "\n"; 
});
```