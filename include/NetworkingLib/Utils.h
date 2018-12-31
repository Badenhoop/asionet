//
// Created by philipp on 02.01.18.
//

#ifndef NETWORKINGLIB_UTILS_H
#define NETWORKINGLIB_UTILS_H

#include <functional>

namespace networking
{
namespace utils
{

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
