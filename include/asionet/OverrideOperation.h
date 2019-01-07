//
// Created by philipp on 07.01.19.
//

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
	explicit OverrideOperation(asionet::Context & context, std::function<void()> stopOperation)
		: context(context), stopOperation(std::move(stopOperation))
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

		stopOperation();
		pendingOperation = std::make_unique<std::function<void()>>(
			[asyncOperation, asyncOperationArgs...]() mutable
			{
				asyncOperation(asyncOperationArgs...);
			});
	}

	void notifyFinishedOperation()
	{
		std::lock_guard<std::recursive_mutex> lock{mutex};
		if (!pendingOperation)
		{
			executing = false;
			return;
		}

		context.post(*pendingOperation);
		pendingOperation = nullptr;
	}

	void cancelPendingOperation()
	{
		std::lock_guard<std::recursive_mutex> lock{mutex};
		pendingOperation = nullptr;
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
				operation.notifyFinishedOperation();
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
			operation.notifyFinishedOperation();
		}

	private:
		OverrideOperation & operation;
		std::atomic<bool> enabled{true};
	};

private:
	asionet::Context & context;
	std::recursive_mutex mutex;
	std::unique_ptr<std::function<void()>> pendingOperation = nullptr;
	bool executing{false};
	std::function<void()> stopOperation;
};

}
}

#endif //ASIONET_OVERRIDEOPERATION_H
