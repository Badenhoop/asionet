//
// Created by philipp on 12.01.18.
//

#ifndef NETWORKINGLIB_RESOLVER_H
#define NETWORKINGLIB_RESOLVER_H

#include <memory>
#include <boost/asio/ip/tcp.hpp>
#include "Error.h"
#include "Utils.h"
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
    using EndpointIterator = typename Resolver::iterator;

    using ResolveHandler = std::function<void(const boost::system::error_code & error)>;

    explicit CloseableResolver(asionet::Context & context)
        : Resolver(context)
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

template<typename Protocol>
class Resolver
{
public:
	using UnderlyingResolver = typename internal::CloseableResolver<Protocol>;
	using ResolveHandler = std::function<void(const error::Error & error,
	                                          const typename UnderlyingResolver::EndpointIterator &)>;

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
	    operationQueue.dispatch(asyncOperation, host, service, timeout, handler);
    }

    void stop()
    {
        closeable::Closer<UnderlyingResolver>::close(resolver);
        operationQueue.cancelQueuedOperations();
    }

private:
    asionet::Context & context;
    UnderlyingResolver resolver;
    utils::OperationQueue operationQueue;

    struct AsyncState
    {
        AsyncState(Resolver & resolver,
                   const ResolveHandler & handler)
            : handler(handler)
              , finishedNotifier(resolver.operationQueue)
        {}

        ResolveHandler handler;
        utils::OperationQueue::FinishedOperationNotifier finishedNotifier;
    };

    void asyncResolveOperation(const std::string & host,
                               const std::string & service,
                               const time::Duration & timeout,
                               const ResolveHandler & handler)
    {
        resolver.open();

        typename UnderlyingResolver::Query query{host, service};

        auto state = std::make_shared<AsyncState>(*this, handler);

        auto resolveOperation = [this](auto && ... args)
        { resolver.async_resolve(std::forward<decltype(args)>(args)...); };

        closeable::timedAsyncOperation(
            context,
            resolveOperation,
            resolver,
            timeout,
            [this, state = std::move(state)](const auto & error, const auto & endpointIterator)
            {
	            state->finishedNotifier.notify();
                state->handler(error, endpointIterator);
            },
            query);
    }
};

}

#endif //NETWORKINGLIB_RESOLVER_H
