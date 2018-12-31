//
// Created by philipp on 02.01.18.
//

#ifndef NETWORKINGLIB_CLOSEABLE_H_H
#define NETWORKINGLIB_CLOSEABLE_H_H

#include "Networking.h"
#include "Timer.h"
#include "Error.h"

namespace networking
{
namespace closeable
{

template<typename Closeable>
class Closer
{
public:
    Closer(Closeable & closeable)
        : closeable(closeable)
    {}

    ~Closer()
    {
        if (alive)
            close(closeable);
    }

    Closer(const Closer &) = delete;

    Closer & operator=(const Closer &) = delete;

    Closer(Closer && other)
        : closeable(other.closeable)
    {
        other.alive = false;
    }

    Closer & operator=(Closer && other)
    {
        closeable = other.closeable;
        alive = other.alive;
        other.alive = false;
        return *this;
    }

    static void close(Closeable & closeable)
    {
        boost::system::error_code ignoredError;
        closeable.close(ignoredError);
    }

private:
    Closeable & closeable;
    bool alive{true};
};

template<typename Closeable>
struct IsOpen
{
    bool operator()(Closeable & closeable) const
    {
        return closeable.is_open();
    }
};

template<
    typename ResultTuple,
    typename AsyncOperation,
    typename... AsyncOperationArgs,
    typename Closeable>
void timedOperation(ResultTuple & result,
                    Networking & net,
                    AsyncOperation asyncOperation,
                    Closeable & closeable,
                    const time::Duration & timeout,
                    AsyncOperationArgs && ... args)
{
    auto & ioService = net.getIoService();

    auto timer = time::Timer::create(net);
    timer->startTimeout(
        timeout,
        [&closeable]
        {
            // Timeout expired: close the closeable.
            Closer<Closeable>::close(closeable);
        });

    // A 'would_block' closeableError is guaranteed to never occur on an asynchronous operation.
    boost::system::error_code closeableError = boost::asio::error::would_block;

    // Run asynchronous connect.
    asyncOperation(
        std::forward<AsyncOperationArgs>(args)...,
        [&closeableError, timer, &result](const boost::system::error_code & error, auto && ... remainingHandlerArgs)
        {
            timer->stop();
            // Create a tuple to store the results.
            result = std::make_tuple(error, remainingHandlerArgs...);
            // Update closeableError variable.
            closeableError = error;
        });

    // Wait until "something happens" with the closeable.
    net.waitUntil([&closeableError]()
                  { return closeableError != boost::asio::error::would_block; });

    // Determine whether a connection was successfully established.
    // Even though our timer handler might have run to close the closeable, the connect operation
    // might have notionally succeeded!
    if (closeableError || !IsOpen<Closeable>{}(closeable))
    {
        if (closeableError == boost::asio::error::operation_aborted)
            throw error::Aborted{};

        throw error::FailedOperation{};
    }
}

template<
    typename AsyncOperation,
    typename... AsyncOperationArgs,
    typename Closeable,
    typename Handler>
void timedAsyncOperation(Networking & net,
                         AsyncOperation asyncOperation,
                         Closeable & closeable,
                         const time::Duration & timeout,
                         const Handler & handler,
                         AsyncOperationArgs && ... asyncOperationArgs)
{
    auto timer = time::Timer::create(net);
    timer->startTimeout(
        timeout,
        [&closeable]
        {
            Closer<Closeable>::close(closeable);
        });

    asyncOperation(
        std::forward<AsyncOperationArgs>(asyncOperationArgs)...,
        [&closeable, timer, handler](const boost::system::error_code & opError, auto && ... remainingHandlerArgs)
        {
            timer->stop();

            auto errorCode = error::codes::SUCCESS;
            if (!IsOpen<Closeable>{}(closeable))
                errorCode = error::codes::ABORTED;
            else if (opError)
                errorCode = error::codes::FAILED_OPERATION;

            handler(errorCode, opError, std::forward<decltype(remainingHandlerArgs)>(remainingHandlerArgs)...);
        });
}

}
}

#endif //NETWORKINGLIB_CLOSEABLE_H_H
