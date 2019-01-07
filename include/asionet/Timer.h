//
// Created by philipp on 28.12.17.
//

#ifndef NETWORKINGLIB_TIMER_H
#define NETWORKINGLIB_TIMER_H

#include <boost/asio.hpp>
#include <boost/asio/steady_timer.hpp>
#include "Time.h"
#include "Context.h"
#include "OverrideOperation.h"

namespace asionet
{

class Timer
{
public:
	using TimeoutHandler = std::function<void()>;

	// Objects of this class should always be declared as std::shared_ptr.
	explicit Timer(asionet::Context & context)
		: timer(context)
		  , overrideOperation(context, [this] { this->stopOperation(); })
	{}

	void startTimeout(time::Duration duration, TimeoutHandler handler)
	{
		auto asyncOperation = [this](auto && ... args)
		{ this->startTimeoutOperation(std::forward<decltype(args)>(args)...); };
		overrideOperation.dispatch(asyncOperation, duration, handler);
	}

	void startPeriodicTimeout(time::Duration interval, TimeoutHandler handler)
	{
		auto asyncOperation = [this](auto && ... args)
		{ this->startPeriodicTimeoutOperation(std::forward<decltype(args)>(args)...); };
		overrideOperation.dispatch(asyncOperation, interval, handler);
	}

	void stop()
	{
		stopOperation();
		overrideOperation.cancelPendingOperation();
	}

private:
	struct AsyncState
	{
		AsyncState(Timer & timer, TimeoutHandler && handler, time::Duration && duration)
			: handler(std::move(handler))
			  , duration(std::move(duration))
			  , notifier(timer.overrideOperation)
		{}

		TimeoutHandler handler;
		time::Duration duration;
		utils::OverrideOperation::FinishedOperationNotifier notifier;
	};

	boost::asio::basic_waitable_timer<time::Clock> timer;
	std::atomic<bool> enabled{true};
	utils::OverrideOperation overrideOperation;

	void startTimeoutOperation(time::Duration & duration, TimeoutHandler & handler)
	{
		enabled = true;

		auto state = std::make_shared<AsyncState>(*this, std::move(handler), std::move(duration));

		timer.expires_from_now(state->duration);
		timer.async_wait(
			[this, state = std::move(state)](const boost::system::error_code & error) mutable
			{
				if (error || !enabled)
					return;

				state->notifier.notify();
				state->handler();
			});
	}

	void startPeriodicTimeoutOperation(time::Duration & interval, TimeoutHandler & handler)
	{
		enabled = true;

		auto state = std::make_shared<AsyncState>(*this, std::move(handler), std::move(interval));

		timer.expires_from_now(state->duration);
		timer.async_wait(
			[this, state = std::move(state)](const boost::system::error_code & error) mutable
			{
				if (error || !enabled)
					return;

				state->handler();
				nextPeriod(state);
			});
	}

	void stopOperation()
	{
		enabled = false;
		boost::system::error_code ignoredError;
		timer.cancel(ignoredError);
	}

	void nextPeriod(std::shared_ptr<AsyncState> & state)
	{
		if (!enabled)
			return;

		timer.expires_at(timer.expires_at() + state->duration);
		timer.async_wait(
			[this, state = std::move(state)](const boost::system::error_code & error) mutable
			{
				if (error || !enabled)
					return;

				state->handler();
				nextPeriod(state);
			});
	}
};

}

#endif //NETWORKINGLIB_TIMER_H
