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
#ifndef ASIONET_CLOSEABLE_H
#define ASIONET_CLOSEABLE_H

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
void timedAsyncOperation(AsyncOperation asyncOperation,
                         Closeable & closeable,
                         const time::Duration & timeout,
                         const Handler & handler,
                         AsyncOperationArgs && ... asyncOperationArgs)
{
	auto & context = closeable.get_executor().context();
	auto serializer = std::make_shared<WorkSerializer>(context);

	auto timer = std::make_shared<Timer>(context);
	timer->startTimeout(
		timeout,
		(*serializer)([&, timer, serializer]
		              {
			              Closer<Closeable>::close(closeable);
		              }));

	asyncOperation(
		std::forward<AsyncOperationArgs>(asyncOperationArgs)...,
		(*serializer)(
			[&, timer, serializer, handler](const boost::system::error_code & boostCode, auto && ... remainingHandlerArgs)
			{
				timer->cancel();

				auto error = error::success;
				if (!IsOpen<Closeable>{}(closeable))
					error = error::aborted;
				else if (boostCode)
					error = error::Error{error::codes::failedOperation, boostCode};

				handler(error, std::forward<decltype(remainingHandlerArgs)>(remainingHandlerArgs)...);
			}));
}

}
}

#endif //ASIONET_CLOSEABLE_H
