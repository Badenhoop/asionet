//
// Created by philipp on 12.01.18.
//

#ifndef NETWORKINGLIB_RESOLVER_H
#define NETWORKINGLIB_RESOLVER_H

#include <memory>
#include <boost/asio/ip/tcp.hpp>
#include "Networking.h"
#include "Error.h"
#include "Utils.h"
#include "Busyable.h"

namespace networking
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
      , private Busyable
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

    static Ptr create(Networking & net)
    {
        return std::make_shared<Resolver>(PrivateTag{}, net);
    }

    Resolver(PrivateTag, Networking & net)
        : net(net)
          , resolver(net.getIoService())
    {}

    std::vector<Endpoint> resolve(const std::string & host,
                                  const std::string & service,
                                  const time::Duration & timeout)
    {
        BusyLock busyLock{*this};
        resolver.open();

        UnderlyingResolver::Query query{host, service};

        auto resolveOperation = [this](auto && ... args)
        { resolver.async_resolve(std::forward<decltype(args)>(args)...); };

        std::tuple<boost::system::error_code, UnderlyingResolver::Iterator> result;

        closeable::timedOperation(
            result,
            net,
            resolveOperation,
            resolver,
            timeout,
            query);

        auto endpointIterator = std::get<1>(result);

        return endpointsFromIterator(endpointIterator);
    }

    void asyncResolve(const std::string & host,
                      const std::string & service,
                      const time::Duration & timeout,
                      const ResolveHandler & handler)
    {
        auto self = shared_from_this();
        auto state = std::make_shared<AsyncState>(self, handler);

        resolver.open();

        UnderlyingResolver::Query query{host, service};

        auto resolveOperation = [this](auto && ... args)
        { resolver.async_resolve(std::forward<decltype(args)>(args)...); };

        closeable::timedAsyncOperation(
            net,
            resolveOperation,
            resolver,
            timeout,
            [state](const auto & networkingError, const auto & boostError, auto endpointIterator)
            {
                state->busyLock.unlock();
                state->handler(networkingError, state->self->endpointsFromIterator(endpointIterator));
            },
            query);
    }

    void stop()
    {
        closeable::Closer<UnderlyingResolver>::close(resolver);
    }

    bool isResolving() const noexcept
    {
        return isBusy();
    }

private:
    using Protocol = boost::asio::ip::tcp;
    using UnderlyingResolver = internal::CloseableResolver<Protocol>;

    Networking & net;
    UnderlyingResolver resolver;

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

    struct AsyncState
    {
        AsyncState(Ptr self, const ResolveHandler & handler)
            : busyLock(*self)
              , self(self)
              , handler(handler)
        {}

        BusyLock busyLock;
        Ptr self;
        ResolveHandler handler;
    };
};

}

#endif //NETWORKINGLIB_RESOLVER_H
