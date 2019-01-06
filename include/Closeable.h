//
// Created by philipp on 02.01.18.
//

#ifndef NETWORKINGLIB_CLOSEABLE_H_H
#define NETWORKINGLIB_CLOSEABLE_H_H

#include "Timer.h"
#include "Error.h"
#include "Context.h"
#include "Utils.h"
#include "WorkSerializer.h"
#include "Wait.h"

namespace asionet
{
namespace closeable
{

template<typename Closeable>
class Closer
{
public:
	explicit Closer(Closeable & closeable)
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
	typename AsyncOperation,
	typename... AsyncOperationArgs,
	typename Closeable,
	typename Handler>
void timedAsyncOperation(asionet::Context & context,
                         AsyncOperation asyncOperation,
                         Closeable & closeable,
                         const time::Duration & timeout,
                         const Handler & handler,
                         AsyncOperationArgs && ... asyncOperationArgs)
{
	auto serializer = std::make_shared<WorkSerializer>(context);

	auto timer = std::make_shared<Timer>(context);
	timer->startTimeout(
		timeout,
		(*serializer)([&, timer]
		              {
			              Closer<Closeable>::close(closeable);
		              }));

	asyncOperation(
		std::forward<AsyncOperationArgs>(asyncOperationArgs)...,
		(*serializer)(
			[&, timer, serializer, handler](const boost::system::error_code & opError, auto && ... remainingHandlerArgs)
			{
				timer->stop();

				auto errorCode = error::codes::SUCCESS;
				if (!IsOpen<Closeable>{}(closeable))
					errorCode = error::codes::ABORTED;
				else if (opError)
					errorCode = error::codes::FAILED_OPERATION;

				handler(errorCode, opError, std::forward<decltype(remainingHandlerArgs)>(remainingHandlerArgs)...);
			}));
}

}
}

#endif //NETWORKINGLIB_CLOSEABLE_H_H
