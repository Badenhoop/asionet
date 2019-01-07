//
// Created by philipp on 12.01.18.
//

#ifndef NETWORKINGLIB_RESOLVER_H
#define NETWORKINGLIB_RESOLVER_H

#include <memory>
#include <boost/asio/ip/tcp.hpp>
#include "Error.h"
#include "Utils.h"
#include "Busyable.h"
#include "Context.h"
#include "OperationQueue.h"

namespace asionet
{

namespace internal
{

// This class is used for internal purposes. Please use the 'Resolver' class.
// The boost::asio resolvers do not feature a close mechanism which we need in order to perform operations with timeouts.
template<typename Protocol>
class CloseableResolver : public Protocol::resolver
{
public:
    using Resolver = typename Protocol::resolver;
    using Query = typename Resolver::query;
    using Iterator = typename Resolver::iterator;

    using ResolveHandler = std::function<void(const boost::system::error_code & error)>;

    CloseableResolver(boost::asio::io_service & ioService)
        : Resolver(ioService)
    {}

    void open()
    {
        opened = true;
    }

    bool is_open() const noexcept
    {
        return opened;
    }

    void close(boost::system::error_code &)
    {
        if (!opened)
            return;

        opened = false;
        Resolver::cancel();
    }

private:
    std::atomic<bool> opened{true};
};

}

class Resolver
{
public:
    struct Endpoint
    {
        Endpoint(const std::string & ip, std::uint16_t port)
            : ip(ip), port(port)
        {}

        std::string ip;
        std::uint16_t port;
    };

    using ResolveHandler = std::function<void(const error::Error & error, const std::vector<Endpoint> & endpoints)>;

    explicit Resolver(asionet::Context & context)
        : context(context)
          , resolver(context)
          , operationQueue(context)
    {}

    void asyncResolve(const std::string & host,
                      const std::string & service,
                      const time::Duration & timeout,
                      const ResolveHandler & handler)
    {
        auto asyncOperation = [this](auto && ... args)
        { this->asyncResolveOperation(std::forward<decltype(args)>(args)...); };
        operationQueue.execute(asyncOperation, handler, host, service, timeout);
    }

    void stop()
    {
        closeable::Closer<UnderlyingResolver>::close(resolver);
        operationQueue.clear();
    }

private:
    using Protocol = boost::asio::ip::tcp;
    using UnderlyingResolver = internal::CloseableResolver<Protocol>;

    asionet::Context & context;
    UnderlyingResolver resolver;
    utils::OperationQueue operationQueue;

    void asyncResolveOperation(const ResolveHandler & handler,
                               const std::string & host,
                               const std::string & service,
                               const time::Duration & timeout)
    {
        resolver.open();

        UnderlyingResolver::Query query{host, service};

        auto resolveOperation = [this](auto && ... args)
        { resolver.async_resolve(std::forward<decltype(args)>(args)...); };

        closeable::timedAsyncOperation(
            context,
            resolveOperation,
            resolver,
            timeout,
            [this, handler](const auto & error, auto endpointIterator)
            {
                handler(error, this->endpointsFromIterator(endpointIterator));
            },
            query);
    }

    std::vector<Endpoint> endpointsFromIterator(UnderlyingResolver::Iterator iterator)
    {
        std::vector<Endpoint> endpoints;

        while (iterator != UnderlyingResolver::Iterator{})
        {
            endpoints.emplace_back(iterator->endpoint().address().to_string(),
                                   iterator->endpoint().port());
            iterator++;
        }

        return endpoints;
    }
};

}

#endif //NETWORKINGLIB_RESOLVER_H
