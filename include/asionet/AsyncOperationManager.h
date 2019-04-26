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
#ifndef ASIONET_QUEUEDEXECUTER_H
#define ASIONET_QUEUEDEXECUTER_H

#include <memory>
#include <queue>
#include "Context.h"
#include "Utils.h"

namespace asionet
{

/**
 * Class used to queue async operations.
 * For example consider the DatagramSender:
 * We must ensure that there NEVER exist more than one send-operations running at the same time.
 * Whenever send() is called and there currently runs another send() operation, we have to queue the incoming send()
 * operation for later execution.
 * QueuedExecutor wraps the handler of an async operation such that whenever a running operation finishes, the next queued
 * operation is automatically posted on the given asionet::Context for execution.
 */
template<typename PendingOperationContainer>
class AsyncOperationManager
{
public:
	explicit AsyncOperationManager(asionet::Context & context, std::function<void()> cancelOperation)
		: context(context), cancelOperation(std::move(cancelOperation))
	{}

	template<typename AsyncOperation, typename ... AsyncOperationArgs>
	void dispatch(const AsyncOperation & asyncOperation, AsyncOperationArgs && ... asyncOperationArgs)
	{
		std::lock_guard<std::recursive_mutex> lock{mutex};

		if (!running)
		{
			running = true;
			asyncOperation(std::forward<decltype(asyncOperationArgs)>(asyncOperationArgs)...);
			return;
		}

		if (pendingOperations.shouldCancel())
			cancelOperation();

		pendingOperations.pushPendingOperation(asyncOperation, asyncOperationArgs...);
	}

	void finish()
	{
		std::lock_guard<std::recursive_mutex> lock{mutex};

		canceled = false;

		if (!pendingOperations.hasPendingOperation())
		{
			running = false;
			return;
		}

		auto pendingOperation = pendingOperations.getPendingOperation();
		pendingOperations.popPendingOperation();
		pendingOperation();
	}

	void cancel()
	{
		std::lock_guard<std::recursive_mutex> lock{mutex};
		canceled = true;
		cancelOperation();
		pendingOperations.reset();
	}

	bool isCanceled() const
	{
		return canceled;
	}

	class FinishedOperationNotifier
	{
	public:
		explicit FinishedOperationNotifier(AsyncOperationManager<PendingOperationContainer> & operationManager)
			: operationManager(operationManager)
		{}

		~FinishedOperationNotifier()
		{
			if (enabled)
				operationManager.finish();
		}

		FinishedOperationNotifier(FinishedOperationNotifier && other) noexcept
			: operationManager(other.operationManager), enabled(other.enabled.load())
		{
			other.enabled = false;
		}

		FinishedOperationNotifier(const FinishedOperationNotifier &) = delete;

		FinishedOperationNotifier & operator=(const FinishedOperationNotifier &) = delete;

		FinishedOperationNotifier & operator=(FinishedOperationNotifier && other) noexcept = delete;

		void notify()
		{
			enabled = false;
			operationManager.finish();
		}

	private:
		AsyncOperationManager<PendingOperationContainer> & operationManager;
		std::atomic<bool> enabled{true};
	};

private:
	asionet::Context & context;
	std::recursive_mutex mutex;
	PendingOperationContainer pendingOperations;
	std::atomic<bool> running{false};
	std::atomic<bool> canceled{false};
	std::function<void()> cancelOperation;
};

class PendingOperationQueue
{
public:
	bool shouldCancel() const
	{
		return false;
	}

	bool hasPendingOperation() const
	{
		return !operations.empty();
	}

	template<typename AsyncOperation, typename ... AsyncOperationArgs>
	void pushPendingOperation(const AsyncOperation & asyncOperation, AsyncOperationArgs && ... asyncOperationArgs)
	{
		operations.push(
			[asyncOperation, asyncOperationArgs...] () mutable
			{
				asyncOperation(asyncOperationArgs...);
			}
		);
	}

	void popPendingOperation()
	{
		operations.pop();
	}

	std::function<void()> getPendingOperation() const
	{
		return operations.front();
	}

	void reset()
	{
		operations = std::queue<std::function<void()>>{};
	};

private:
	std::queue<std::function<void()>> operations;
};

class PendingOperationReplacer
{
public:
	bool shouldCancel() const
	{
		return true;
	}

	bool hasPendingOperation() const
	{
		return operation != nullptr;
	}

	template<typename AsyncOperation, typename ... AsyncOperationArgs>
	void pushPendingOperation(const AsyncOperation & asyncOperation, AsyncOperationArgs && ... asyncOperationArgs)
	{
		operation = std::make_unique<std::function<void()>>(
			[asyncOperation, asyncOperationArgs...]() mutable
			{
				asyncOperation(asyncOperationArgs...);
			});
	}

	void popPendingOperation()
	{
		operation = nullptr;
	}

	std::function<void()> getPendingOperation() const
	{
		return *operation;
	}

	void reset()
	{
		operation = nullptr;
	}

private:
	std::unique_ptr<std::function<void()>> operation = nullptr;
};

}

#endif //ASIONET_QUEUEDEXECUTER_H
