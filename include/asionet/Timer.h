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
#ifndef ASIONET_TIMER_H
#define ASIONET_TIMER_H

#include <boost/asio.hpp>
#include <boost/asio/steady_timer.hpp>
#include "Time.h"
#include "Context.h"
#include "AsyncOperationManager.h"

namespace asionet
{

class Timer
{
public:
	using TimeoutHandler = std::function<void()>;

	// Objects of this class should always be declared as std::shared_ptr.
	explicit Timer(asionet::Context & context)
		: timer(context)
		  , operationManager(context, [this] { this->cancelOperation(); })
	{}

	void startTimeout(time::Duration duration, TimeoutHandler handler)
	{
		auto asyncOperation = [this](auto && ... args)
		{ this->startTimeoutOperation(std::forward<decltype(args)>(args)...); };
		operationManager.startOperation(asyncOperation, duration, handler);
	}

	void startPeriodicTimeout(time::Duration interval, TimeoutHandler handler)
	{
		auto asyncOperation = [this](auto && ... args)
		{ this->startPeriodicTimeoutOperation(std::forward<decltype(args)>(args)...); };
		operationManager.startOperation(asyncOperation, interval, handler);
	}

	void cancel()
	{
		operationManager.cancelOperation();
	}

private:
	struct AsyncState
	{
		AsyncState(Timer & timer, TimeoutHandler && handler, time::Duration && duration)
			: handler(std::move(handler))
			  , duration(std::move(duration))
			  , notifier(timer.operationManager)
		{}

		TimeoutHandler handler;
		time::Duration duration;
		AsyncOperationManager<PendingOperationReplacer>::FinishedOperationNotifier notifier;
	};

	boost::asio::basic_waitable_timer<time::Clock> timer;
	AsyncOperationManager<PendingOperationReplacer> operationManager;

	void startTimeoutOperation(time::Duration & duration, TimeoutHandler & handler)
	{
		auto state = std::make_shared<AsyncState>(*this, std::move(handler), std::move(duration));

		timer.expires_from_now(state->duration);
		timer.async_wait(
			[this, state = std::move(state)](const boost::system::error_code & error) mutable
			{
				if (error || operationManager.isCanceled())
					return;

				state->notifier.notify();
				state->handler();
			});
	}

	void startPeriodicTimeoutOperation(time::Duration & interval, TimeoutHandler & handler)
	{
		auto state = std::make_shared<AsyncState>(*this, std::move(handler), std::move(interval));

		timer.expires_from_now(state->duration);
		timer.async_wait(
			[this, state = std::move(state)](const boost::system::error_code & error) mutable
			{
				if (error || operationManager.isCanceled())
					return;

				state->handler();
				nextPeriod(state);
			});
	}

	void cancelOperation()
	{
		boost::system::error_code ignoredError;
		timer.cancel(ignoredError);
	}

	void nextPeriod(std::shared_ptr<AsyncState> & state)
	{
		if (operationManager.isCanceled())
			return;

		timer.expires_at(timer.expires_at() + state->duration);
		timer.async_wait(
			[this, state = std::move(state)](const boost::system::error_code & error) mutable
			{
				if (error || operationManager.isCanceled())
					return;

				state->handler();
				nextPeriod(state);
			});
	}
};

}

#endif //ASIONET_TIMER_H
