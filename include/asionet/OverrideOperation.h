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
#ifndef ASIONET_OVERRIDEOPERATION_H
#define ASIONET_OVERRIDEOPERATION_H

#include "Context.h"
#include "Monitor.h"

namespace asionet
{
namespace utils
{

class OverrideOperation
{
public:
	explicit OverrideOperation(asionet::Context & context, std::function<void()> cancelOperation)
		: context(context), cancelOperation(std::move(cancelOperation))
	{}

	template<typename AsyncOperation, typename ... AsyncOperationArgs>
	void dispatch(const AsyncOperation & asyncOperation, AsyncOperationArgs && ... asyncOperationArgs)
	{
		std::lock_guard<std::recursive_mutex> lock{mutex};
		if (!executing)
		{
			executing = true;
			asyncOperation(std::forward<decltype(asyncOperationArgs)>(asyncOperationArgs)...);
			return;
		}

		cancelOperation();
		pendingOperation = std::make_unique<std::function<void()>>(
			[asyncOperation, asyncOperationArgs...]() mutable
			{
				asyncOperation(asyncOperationArgs...);
			});
	}

	void notifyFinished()
	{
		std::lock_guard<std::recursive_mutex> lock{mutex};
		canceled = false;
		if (!pendingOperation)
		{
			executing = false;
			return;
		}

		auto operation = std::move(pendingOperation);
		pendingOperation = nullptr;
		(*operation)();
	}

	void cancel()
	{
		std::lock_guard<std::recursive_mutex> lock{mutex};
		canceled = true;
		cancelOperation();
		pendingOperation = nullptr;
	}

	bool isCanceled() const
	{
		return canceled;
	}

	class FinishedOperationNotifier
	{
	public:
		explicit FinishedOperationNotifier(OverrideOperation & operation)
			: operation(operation)
		{}

		~FinishedOperationNotifier()
		{
			if (enabled)
				operation.notifyFinished();
		}

		FinishedOperationNotifier(FinishedOperationNotifier && other) noexcept
			: operation(other.operation), enabled(other.enabled.load())
		{
			other.enabled = false;
		}

		FinishedOperationNotifier(const FinishedOperationNotifier &) = delete;

		FinishedOperationNotifier & operator=(const FinishedOperationNotifier &) = delete;

		FinishedOperationNotifier & operator=(FinishedOperationNotifier && other) noexcept = delete;

		void notify()
		{
			enabled = false;
			operation.notifyFinished();
		}

	private:
		OverrideOperation & operation;
		std::atomic<bool> enabled{true};
	};

private:
	asionet::Context & context;
	std::recursive_mutex mutex;
	std::unique_ptr<std::function<void()>> pendingOperation = nullptr;
	std::atomic<bool> executing{false};
	std::atomic<bool> canceled{false};
	std::function<void()> cancelOperation;
};

}
}

#endif //ASIONET_OVERRIDEOPERATION_H
