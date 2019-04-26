/*
 * The MIT License
 * 
 * Copyright (c) 2019 Philipp Badenhoop
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#ifndef ASIONET_RESOLVER_H
#define ASIONET_RESOLVER_H

#include <memory>
#include <boost/asio/ip/tcp.hpp>
#include "Error.h"
#include "Utils.h"
#include "Context.h"
#include "AsyncOperationManager.h"

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
          , operationManager(context, [this] { this->cancelOperation(); })
    {}

    void asyncResolve(const std::string & host,
                      const std::string & service,
                      const time::Duration & timeout,
                      const ResolveHandler & handler)
    {
        auto asyncOperation = [this](auto && ... args)
        { this->asyncResolveOperation(std::forward<decltype(args)>(args)...); };
	    operationManager.startOperation(asyncOperation, host, service, timeout, handler);
    }

    void stop()
    {
	    operationManager.cancelOperation();
    }

private:
    asionet::Context & context;
    UnderlyingResolver resolver;
    AsyncOperationManager<PendingOperationQueue> operationManager;

    struct AsyncState
    {
        AsyncState(Resolver & resolver,
                   const ResolveHandler & handler)
            : handler(handler)
              , finishedNotifier(resolver.operationManager)
        {}

        ResolveHandler handler;
	    AsyncOperationManager<PendingOperationQueue>::FinishedOperationNotifier finishedNotifier;
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

    void cancelOperation()
    {
	    closeable::Closer<UnderlyingResolver>::close(resolver);
    }
};

}

#endif //ASIONET_RESOLVER_H
