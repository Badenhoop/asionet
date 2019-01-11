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
#include "../include/asionet/Wait.h"

namespace asionet
{

void Waiter::await(const Waitable & waitable)
{
	await([&] { return waitable.ready; });
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