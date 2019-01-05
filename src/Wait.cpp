//
// Created by philipp on 05.01.19.
//

#include "../include/Wait.h"

namespace asionet
{

void Waiter::await(const Waitable & waitable)
{
	await([&] { return waitable.done; });
}

void Waiter::await(const WaitExpression & expression)
{
	// We want to wait until the condition becomes true.
	// So during our waiting, we have to run the context. But there are two cases to consider:
	// We were called from a thread which is currently running the context object:
	//      In this case we must invoke context.run_one() to ensure that further handlers can be invoked.
	// Else:
	//      Block the current thread until we get notified from a waitable.
	if (context.get_executor().running_in_this_thread())
	{
		for (;;)
		{
			{
				std::lock_guard<std::mutex> lock{mutex};
				if (expression())
					break;
			}
			if (context.stopped())
				break;
			context.run_one();
		}
	}
	else
	{
		std::unique_lock<std::mutex> lock{mutex};
		while (!expression() && !context.stopped())
		{
			cond.wait(lock);
		}
	}
}

}