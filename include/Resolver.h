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
#include "QueuedExecutor.h"

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
    : public std::enable_shared_from_this<Resolver>
{
private:
    struct PrivateTag
    {
    };

public:
    struct Endpoint
    {
        Endpoint(const std::string & ip, std::uint16_t port)
            : ip(ip), port(port)
        {}

        std::string ip;
        std::uint16_t port;
    };

    using Ptr = std::shared_ptr<Resolver>;

    using ResolveHandler = std::function<void(const error::ErrorCode & error, const std::vector<Endpoint> & endpoints)>;

    static Ptr create(asionet::Context & context)
    {
        return std::make_shared<Resolver>(PrivateTag{}, context);
    }

    Resolver(PrivateTag, asionet::Context & context)
        : context(context)
          , resolver(context)
          , queuedExecutor(utils::QueuedExecutor::create(context))
    {}

    void asyncResolve(const std::string & host,
                      const std::string & service,
                      const time::Duration & timeout,
                      const ResolveHandler & handler)
    {
        auto self = this->shared_from_this();
        auto asyncOperation = [self](auto && ... args)
        { self->asyncResolveOperation(std::forward<decltype(args)>(args)...); };
        queuedExecutor->execute(asyncOperation, handler, host, service, timeout);
    }

    void stop()
    {
        closeable::Closer<UnderlyingResolver>::close(resolver);
        queuedExecutor->clear();
    }

private:
    using Protocol = boost::asio::ip::tcp;
    using UnderlyingResolver = internal::CloseableResolver<Protocol>;

    asionet::Context & context;
    UnderlyingResolver resolver;
    utils::QueuedExecutor::Ptr queuedExecutor;

    void asyncResolveOperation(const ResolveHandler & handler,
                               const std::string & host,
                               const std::string & service,
                               const time::Duration & timeout)
    {
        auto self = shared_from_this();
        resolver.open();

        UnderlyingResolver::Query query{host, service};

        auto resolveOperation = [this](auto && ... args)
        { resolver.async_resolve(std::forward<decltype(args)>(args)...); };

        closeable::timedAsyncOperation(
            context,
            resolveOperation,
            resolver,
            timeout,
            [self, handler](const auto & networkingError, const auto & boostError, auto endpointIterator)
            {
                handler(networkingError, self->endpointsFromIterator(endpointIterator));
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
