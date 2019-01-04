//
// Created by philipp on 03.01.19.
//

#ifndef ASIONET_QUEUEDEXECUTER_H
#define ASIONET_QUEUEDEXECUTER_H

#include <memory>
#include <queue>
#include "Context.h"
#include "Utils.h"
#include "Monitor.h"

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
class QueuedExecutor
	: public std::enable_shared_from_this<QueuedExecutor>
{
private:
	struct PrivateTag
	{
	};

public:
	using Ptr = std::shared_ptr<QueuedExecutor>;

	static Ptr create(asionet::Context & context)
	{
		return std::make_shared<QueuedExecutor>(PrivateTag{}, context);
	}

	QueuedExecutor(PrivateTag, asionet::Context & context)
		: context(context)
	{}

	template<typename AsyncOperation, typename Handler, typename ... AsyncOperationArgs>
	void execute(const AsyncOperation & asyncOperation,
	             const Handler & handler,
	             AsyncOperationArgs && ... asyncOperationArgs)
	{
		auto self = shared_from_this();

		// Wrap the handler inside another handler.
		// This wrapped handler executes the passed handler and checks whether there exist any further operations which are waiting inside the queue.
		// If this is the case, the operation will be popped from the queue and posted to the context for later execution.
		auto wrappedHandler = [self, handler](auto && ... handlerArgs)
		{
			handler(std::forward<decltype(handlerArgs)>(handlerArgs)...);
			self->operationQueue(
				[&](auto & queue)
				{
					if (!queue.empty())
					{
						auto nextOperation = queue.front();
						queue.pop();
						self->context.post(nextOperation);
					}
				});
		};

		// If queue is empty, we can call the wrapped handler directly.
		// Else we create an operation handler which executes the passed async operation along with its arguments.
		// This operation is pushed to the queue for later invocation.
		operationQueue(
			[&](auto & queue)
			{
				if (queue.empty())
				{
					asyncOperation(wrappedHandler, std::forward<AsyncOperationArgs>(asyncOperationArgs)...);
					return;
				}

				queue.push(
					[asyncOperation, wrappedHandler, asyncOperationArgs...]
					{
						asyncOperation(wrappedHandler, asyncOperationArgs...);
					}
				);
			});


	}

	void clear()
	{
		operationQueue(
			[&](auto & queue)
			{
				queue = std::queue<std::function<void()>>{};
			});
	}

private:
	asionet::Context & context;
	utils::Monitor<std::queue<std::function<void()>>> operationQueue;
};

}
}

#endif //ASIONET_QUEUEDEXECUTER_H
