//
// Created by philipp on 03.01.19.
//

#ifndef ASIONET_QUEUEDEXECUTER_H
#define ASIONET_QUEUEDEXECUTER_H

#include <memory>
#include <queue>
#include "Context.h"
#include "Utils.h"

namespace asionet
{
namespace utils
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
class OperationQueue
{
public:
	explicit OperationQueue(asionet::Context & context)
		: context(context)
	{}

	template<typename AsyncOperation, typename ... AsyncOperationArgs>
	void dispatch(const AsyncOperation & asyncOperation, AsyncOperationArgs && ... asyncOperationArgs)
	{
		// It could be absolutely possible that in asyncOperation someone access methods
		// of this object within the same thread. This is why we need a recursive mutex.
		std::lock_guard<std::recursive_mutex> lock{mutex};

		// If queue is empty, we can call the wrapped handler directly.
		// Else we create an operation handler which executes the passed async operation along with its arguments.
		// This operation is pushed to the queue for later invocation.
		if (!executing)
		{
			executing = true;
			asyncOperation(std::forward<decltype(asyncOperationArgs)>(asyncOperationArgs)...);
			return;
		}

		operationQueue.push(
			[asyncOperation, asyncOperationArgs...] () mutable
			{
				asyncOperation(asyncOperationArgs...);
			}
		);
	}

	void notifyFinishedOperation()
	{
		std::lock_guard<std::recursive_mutex> lock{mutex};
		if (operationQueue.empty())
		{
			executing = false;
			return;
		}

		context.post(operationQueue.front());
		operationQueue.pop();
	}

	void cancelQueuedOperations()
	{
		std::lock_guard<std::recursive_mutex> lock{mutex};
		operationQueue = std::queue<std::function<void()>>{};
	}

	class FinishedOperationNotifier
	{
	public:
		explicit FinishedOperationNotifier(OperationQueue & queue)
			: queue(queue)
		{}

		~FinishedOperationNotifier()
		{
			if (enabled)
				queue.notifyFinishedOperation();
		}

		FinishedOperationNotifier(FinishedOperationNotifier && other) noexcept
			: queue(other.queue), enabled(other.enabled.load())
		{
			other.enabled = false;
		}

		FinishedOperationNotifier(const FinishedOperationNotifier &) = delete;

		FinishedOperationNotifier & operator=(const FinishedOperationNotifier &) = delete;

		FinishedOperationNotifier & operator=(FinishedOperationNotifier && other) noexcept = delete;

		void notify()
		{
			enabled = false;
			queue.notifyFinishedOperation();
		}

	private:
		OperationQueue & queue;
		std::atomic<bool> enabled{true};
	};

private:
	asionet::Context & context;
	std::recursive_mutex mutex;
	std::queue<std::function<void()>> operationQueue;
	bool executing{false};
};

}
}

#endif //ASIONET_QUEUEDEXECUTER_H
