//
// Created by philipp on 02.12.17.
//

#ifndef PROTOCOL_NETWORKUTILS_H
#define PROTOCOL_NETWORKUTILS_H

#include <boost/asio/ip/tcp.hpp>
#include <boost/asio.hpp>
#include "Timer.h"
#include "Error.h"
#include "Closeable.h"
#include "Frame.h"
#include "Utils.h"

namespace asionet
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
void asyncWrite(asionet::Context & context,
                SyncWriteStream & stream,
                const std::string & writeData,
                const time::Duration & timeout,
                const WriteHandler & handler)
{
    using namespace asionet::internal;
    auto frame = std::make_shared<Frame>((const std::uint8_t *) writeData.c_str(), writeData.size());
    auto buffers = frame->getBuffers();

    auto asyncOperation = [](auto && ... args) { boost::asio::async_write(std::forward<decltype(args)>(args)...); };

    closeable::timedAsyncOperation(
        context, asyncOperation, stream, timeout,
        [handler, frame = std::move(frame)](const auto & networkingError, const auto & boostError, auto numBytesTransferred)
        {
            if (numBytesTransferred < frame->getSize())
            {
                handler(error::codes::FAILED_OPERATION);
                return;
            }

            handler(networkingError);
        },
        stream, buffers);
}

template<typename SyncReadStream>
void asyncRead(asionet::Context & context,
               SyncReadStream & stream,
               boost::asio::streambuf & buffer,
               const time::Duration & timeout,
               const ReadHandler & handler)
{
    using asionet::internal::Frame;
    using namespace asionet::stream::internal;

    auto startTime = time::now();

    auto asyncOperation = [](auto && ... args) { boost::asio::async_read(std::forward<decltype(args)>(args)...); };

    // Receive frame header.
    closeable::timedAsyncOperation(
        context, asyncOperation, stream, timeout,
        [&context, &stream, &buffer, timeout, handler, startTime]
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

	        auto asyncOperation = [](auto && ... args) { boost::asio::async_read(std::forward<decltype(args)>(args)...); };

            // Receive actual data.
            closeable::timedAsyncOperation(
                context, asyncOperation, stream, newTimeout,
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
