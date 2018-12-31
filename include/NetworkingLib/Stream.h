//
// Created by philipp on 02.12.17.
//

#ifndef PROTOCOL_NETWORKUTILS_H
#define PROTOCOL_NETWORKUTILS_H

#include <boost/asio/ip/tcp.hpp>
#include <boost/asio.hpp>
#include "Networking.h"
#include "Timer.h"
#include "Error.h"
#include "Closeable.h"
#include "Frame.h"
#include "Utils.h"

namespace networking
{
namespace stream
{

namespace internal
{

inline std::uint32_t numDataBytesFromBuffer(boost::asio::streambuf & streambuf)
{
    auto numDataBytesStr = utils::stringFromStreambuf(streambuf, 4);
    return utils::fromBigEndian<4, std::uint32_t>((const std::uint8_t *) numDataBytesStr.c_str());
}

}

using WriteHandler = std::function<void(const error::ErrorCode & error)>;

using ReadHandler = std::function<void(const error::ErrorCode & error, std::string & data)>;

template<typename SyncWriteStream>
void write(Networking & net,
           SyncWriteStream & stream,
           const std::string & writeData,
           const time::Duration & timeout)
{
    using namespace networking::internal;
    Frame frame{(const std::uint8_t *) writeData.c_str(), (std::uint32_t) writeData.size()};

    auto asyncOperation = [](auto && ... args) { boost::asio::async_write(std::forward<decltype(args)>(args)...); };

    std::tuple<boost::system::error_code, std::size_t> result;
    closeable::timedOperation(result, net, asyncOperation, stream, timeout, stream, frame.getBuffers());

    auto numBytesTransferred = std::get<1>(result);
    if (numBytesTransferred < frame.getSize())
        throw error::FailedOperation{};
};

template<typename SyncWriteStream>
void asyncWrite(Networking & net,
                SyncWriteStream & stream,
                const std::string & writeData,
                const time::Duration & timeout,
                const WriteHandler & handler)
{
    using namespace networking::internal;
    auto frame = std::make_shared<Frame>((const std::uint8_t *) writeData.c_str(), writeData.size());

    auto asyncOperation = [](auto && ... args) { boost::asio::async_write(std::forward<decltype(args)>(args)...); };

    closeable::timedAsyncOperation(
        net, asyncOperation, stream, timeout,
        [handler, frame](const auto & networkingError, const auto & boostError, auto numBytesTransferred)
        {
            if (numBytesTransferred < frame->getSize())
            {
                handler(error::codes::FAILED_OPERATION);
                return;
            }

            handler(networkingError);
        },
        stream, frame->getBuffers());
}

template<typename SyncReadStream>
std::string read(Networking & net,
                 SyncReadStream & stream,
                 boost::asio::streambuf & buffer,
                 time::Duration timeout)
{
    using networking::internal::Frame;
    using namespace networking::stream::internal;

    auto startTime = time::now();

    auto asyncOperation = [](auto && ... args) { boost::asio::async_read(std::forward<decltype(args)>(args)...); };

    std::tuple<boost::system::error_code, std::size_t> result;

    // Receive frame header.
    closeable::timedOperation(
        result, net, asyncOperation, stream, timeout, stream, buffer,
        boost::asio::transfer_exactly(Frame::HEADER_SIZE));

    auto numBytesTransferred = std::get<1>(result);
    if (numBytesTransferred != Frame::HEADER_SIZE)
        throw error::FailedOperation{};

    auto numDataBytes = numDataBytesFromBuffer(buffer);
    if (numDataBytes == 0)
        return utils::stringFromStreambuf(buffer, numDataBytes);

    timeout -= time::now() - startTime;

    // Receive actual data.
    closeable::timedOperation(
        result, net, asyncOperation, stream, timeout, stream, buffer, boost::asio::transfer_exactly(numDataBytes));

    numBytesTransferred = std::get<1>(result);
    if (numBytesTransferred != numDataBytes)
        throw error::FailedOperation{};

    return utils::stringFromStreambuf(buffer, numDataBytes);
};

template<typename SyncReadStream>
void asyncRead(Networking & net,
               SyncReadStream & stream,
               boost::asio::streambuf & buffer,
               const time::Duration & timeout,
               const ReadHandler & handler)
{
    using networking::internal::Frame;
    using namespace networking::stream::internal;

    auto startTime = time::now();

    auto asyncOperation = [](auto && ... args) { boost::asio::async_read(std::forward<decltype(args)>(args)...); };

    // Receive frame header.
    closeable::timedAsyncOperation(
        net, asyncOperation, stream, timeout,
        [&net, &stream, &buffer, timeout, handler, startTime, asyncOperation]
            (const auto & networkingError, const auto & boostError, auto numBytesTransferred)
        {
            std::string data{};

            if (networkingError)
            {
                handler(networkingError, data);
                return;
            }

            if (numBytesTransferred != Frame::HEADER_SIZE)
            {
                handler(error::codes::FAILED_OPERATION, data);
                return;
            }

            auto numDataBytes = numDataBytesFromBuffer(buffer);
            if (numDataBytes == 0)
            {
                handler(networkingError, data);
                return;
            }

            auto timeSpend = time::now() - startTime;
            auto newTimeout = timeout - timeSpend;

            // Receive actual data.
            closeable::timedAsyncOperation(
                net, asyncOperation, stream, newTimeout,
                [&buffer, handler, numDataBytes]
                    (const auto & networkingError, const auto & boostError, auto numBytesTransferred)
                {
                    std::string data{};

                    if (networkingError)
                    {
                        handler(networkingError, data);
                        return;
                    }

                    if (numBytesTransferred != numDataBytes)
                    {
                        handler(error::codes::FAILED_OPERATION, data);
                        return;
                    }

                    data = utils::stringFromStreambuf(buffer, numDataBytes);
                    handler(networkingError, data);
                },
                stream, buffer, boost::asio::transfer_exactly(numDataBytes));
        },
        stream, buffer, boost::asio::transfer_exactly(Frame::HEADER_SIZE));
};

}
}

#endif //PROTOCOL_NETWORKUTILS_H
