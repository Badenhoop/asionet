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
#ifndef ASIONET_WAITER_H
#define ASIONET_WAITER_H

#include <mutex>
#include <condition_variable>
#include "Context.h"

namespace asionet
{

using WaitExpression = std::function<bool()>;
class Waitable;

class Waiter
{
public:
	friend class Waitable;

	explicit Waiter(asionet::Context & context)
		: context(context)
	{}

	void await(const Waitable & waitable);

	void await(const WaitExpression & expression);

private:
	asionet::Context & context;
	std::mutex mutex;
	std::condition_variable cond;
};

class Waitable
{
public:
	friend class Waiter;

	explicit Waitable(Waiter & waiter)
		: waiter(waiter)
	{}

	template<typename Handler>
	auto operator()(Handler && handler)
	{
		return [handler, this](auto && ... args) -> void
		{
			handler(std::forward<decltype(args)>(args)...);
			this->setReady();
		};
	}

	void setReady()
	{
		{
			std::lock_guard<std::mutex> lock{waiter.mutex};
			ready = true;
		}

		waiter.cond.notify_all();
	}

	void setWaiting()
	{
		std::lock_guard<std::mutex> lock{waiter.mutex};
		ready = false;
	}

	friend WaitExpression operator&&(const Waitable & lhs, const Waitable & rhs)
	{
		return [&] { return lhs.ready && rhs.ready; };
	}

	friend WaitExpression operator&&(const WaitExpression & lhs, const Waitable & rhs)
	{
		return [&] { return lhs() && rhs.ready; };
	}

	friend WaitExpression operator&&(const Waitable & lhs, const WaitExpression & rhs)
	{
		return [&] { return lhs.ready && rhs(); };
	}

	friend WaitExpression operator&&(const WaitExpression & lhs, const WaitExpression & rhs)
	{
		return [&] { return lhs() && rhs(); };
	}

	friend WaitExpression operator||(const Waitable & lhs, const Waitable & rhs)
	{
		return [&] { return lhs.ready || rhs.ready; };
	}

	friend WaitExpression operator||(const WaitExpression & lhs, const Waitable & rhs)
	{
		return [&] { return lhs() || rhs.ready; };
	}

	friend WaitExpression operator||(const Waitable & lhs, const WaitExpression & rhs)
	{
		return [&] { return lhs.ready || rhs(); };
	}

	friend WaitExpression operator||(const WaitExpression & lhs, const WaitExpression & rhs)
	{
		return [&] { return lhs() || rhs(); };
	}

private:
	Waiter & waiter;
	bool ready{false};
};

}

#endif //ASIONET_WAITER_H
