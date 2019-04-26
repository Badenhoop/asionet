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
 * Class used help managing the execution of sequential calls of asynchronous operations.
 * For example, if the user calls the asyncSend() method of DatagramSender n times in a row, then using this class we can
 * start the first call directly and store the rest of the n-1 operations for later execution.
 * More precisely, operation i+1 will be executed directly after operation i has finished its asynchronous execution.
 *
 * Sequentializing arbitrary asynchronous operations is done by defining a start and an end of a given operation.
 * The user must start the asynchronous operation with startOperation() and notify its end using finishOperation().
 * Any asynchronous operation must be cancelable at any given time. Therefore, a canceling operation must be specfied.
 * Canceling an asynchronous operation shall be done by calling the cancelOperation() method.
 *
 * It is important to know that by calling finishOperation() any the next pending operation is directly executed.
 * This means at this point, all resources and state should be already prepared for the next operation.
 *
 * One may use a FinishOperationNotifier instance which can be passed inside a lambda closure of an asynchronous handler.
 * If the FinishOperationNotifier is destructed and its notify() method hasn't already been called, it automatically calls
 * finishOperation() of its assigned AsyncOperationManager instance.
 *
 * The PendingOperationContainer template parameter is used to specify the strategy in which pending operations are stored
 * and retrieved. One may use a PendingOperationQueue which enqueues pending operations or one may choose a
 * PendingOperationReplacer which allows for only a operation at a time which is replaced with each new operation.
 * PendingOperationReplacer is used in DatagramReceiver, Timer and ServiceServer.
 * PendingOperationQueue is used in DatagramSender, ServiceClient and Resolver.
 */
template<typename PendingOperationContainer>
class AsyncOperationManager
{
public:
	explicit AsyncOperationManager(asionet::Context & context, std::function<void()> cancelingOperation)
		: context(context), cancelingOperation(std::move(cancelingOperation))
	{}

	template<typename AsyncOperation, typename ... AsyncOperationArgs>
	void startOperation(const AsyncOperation & asyncOperation, AsyncOperationArgs && ... asyncOperationArgs)
	{
		std::lock_guard<std::recursive_mutex> lock{mutex};

		if (!running)
		{
			running = true;
			asyncOperation(std::forward<decltype(asyncOperationArgs)>(asyncOperationArgs)...);
			return;
		}

		if (pendingOperations.shouldCancel())
			cancelingOperation();

		pendingOperations.pushPendingOperation(asyncOperation, asyncOperationArgs...);
	}

	void finishOperation()
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

	void cancelOperation()
	{
		std::lock_guard<std::recursive_mutex> lock{mutex};
		canceled = true;
		cancelingOperation();
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
				operationManager.finishOperation();
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
			operationManager.finishOperation();
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
	std::function<void()> cancelingOperation;
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
