//
// Created by philipp on 04.01.19.
//

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
	Handler operator()(Handler && handler)
	{
		return [handler, this](auto && ... args)
		{
			handler(args...);

			{
				std::lock_guard<std::mutex> lock{waiter.mutex};
				done = true;
			}

			waiter.cond.notify_all();
		};
	}

	void clear()
	{
		std::lock_guard<std::mutex> lock{waiter.mutex};
		done = false;
	}

	friend WaitExpression operator&&(const Waitable & lhs, const Waitable & rhs)
	{
		return [&] { return lhs.done && rhs.done; };
	}

	friend WaitExpression operator&&(const WaitExpression & lhs, const Waitable & rhs)
	{
		return [&] { return lhs() && rhs.done; };
	}

	friend WaitExpression operator&&(const Waitable & lhs, const WaitExpression & rhs)
	{
		return [&] { return lhs.done && rhs(); };
	}

	friend WaitExpression operator&&(const WaitExpression & lhs, const WaitExpression & rhs)
	{
		return [&] { return lhs() && rhs(); };
	}

	friend WaitExpression operator||(const Waitable & lhs, const Waitable & rhs)
	{
		return [&] { return lhs.done || rhs.done; };
	}

	friend WaitExpression operator||(const WaitExpression & lhs, const Waitable & rhs)
	{
		return [&] { return lhs() || rhs.done; };
	}

	friend WaitExpression operator||(const Waitable & lhs, const WaitExpression & rhs)
	{
		return [&] { return lhs.done || rhs(); };
	}

	friend WaitExpression operator||(const WaitExpression & lhs, const WaitExpression & rhs)
	{
		return [&] { return lhs() || rhs(); };
	}

private:
	Waiter & waiter;
	bool done{false};
};

}

#endif //ASIONET_WAITER_H
