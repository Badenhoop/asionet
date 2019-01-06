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
