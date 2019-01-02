//
// Created by philipp on 02.01.18.
//

#ifndef NETWORKINGLIB_UTILS_H
#define NETWORKINGLIB_UTILS_H

#include <functional>

namespace asionet
{
namespace utils
{

using Condition = std::function<bool()>;

struct WaitCondition
{
	explicit WaitCondition(const Condition & condition)
		: condition(condition)
	{}

	std::mutex mutex;
	std::condition_variable variable;
	Condition condition;
};

inline void waitUntil(asionet::Context & context, WaitCondition & waitCondition)
{
	// This one is quite tricky:
	// We want to wait until the condition becomes true.
	// So during our waiting, we have to run context. But there are two cases to consider:
	// We were called from an ioService handler and therefor from the context-thread:
	//      In this case we must invoke context.run_one() to ensure that further handlers can be invoked.
	// Else we were not called from the context-thread:
	//      Since we must not call context.run_one() from a different thread (since we assume context.run() permanently
	//      runs already on the context-thread, we just wait until the error changed "magically".
	if (context.get_executor().running_in_this_thread())
	{
		while (!context.stopped() && !waitCondition.condition())
			context.run_one();
	}
	else
	{
		std::unique_lock<std::mutex> lock{waitCondition.mutex};
		while (!context.stopped() && !waitCondition.condition())
		{
			waitCondition.variable.wait(lock);
		}
	}
}

template<std::size_t numBytes, typename Int>
inline void toBigEndian(std::uint8_t * dest, Int src)
{
    std::size_t bitsToShift = numBytes * 8;
    for (std::size_t i = 0; i < numBytes; i++)
    {
        bitsToShift -= 8;
        dest[i] = (std::uint8_t) ((src >> bitsToShift) & 0x000000ff);
    }
};

template<std::size_t numBytes, typename Int>
inline Int fromBigEndian(const std::uint8_t * bytes)
{
    Int result = 0;
    std::size_t bitsToShift = numBytes * 8;
    for (std::size_t i = 0; i < numBytes; i++)
    {
        bitsToShift -= 8;
        result += ((Int) bytes[i]) << bitsToShift;
    }
    return result;
}

inline std::string stringFromStreambuf(boost::asio::streambuf & streambuf, std::size_t numBytes)
{
    auto buffers = streambuf.data();
    std::string data{boost::asio::buffers_begin(buffers),
                     boost::asio::buffers_begin(buffers) + numBytes};
    streambuf.consume(numBytes);
    return data;
}

// Generic object which calls a callback-function on destruction.
class RAIIObject
{
public:
    using OnDestructionCallback = std::function<void()>;

    RAIIObject(OnDestructionCallback onDestructionCallback)
        : onDestructionCallback(onDestructionCallback)
    {}

    ~RAIIObject()
    {
        try
        { onDestructionCallback(); }
        catch (...)
        {}
    }

    RAIIObject(const RAIIObject & other) = delete;

    RAIIObject & operator=(const RAIIObject & other) = delete;

    RAIIObject(RAIIObject && other)
        : onDestructionCallback(other.onDestructionCallback)
    {
        other.onDestructionCallback = []{};
    }

    RAIIObject & operator=(RAIIObject && other)
    {
        onDestructionCallback = other.onDestructionCallback;
        other.onDestructionCallback = []{};
    }

private:
    OnDestructionCallback onDestructionCallback;
};

}
}

#endif //NETWORKINGLIB_UTILS_H
