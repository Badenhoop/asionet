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
#ifndef ASIONET_WORKERPOOL_H
#define ASIONET_WORKERPOOL_H

#include <boost/asio/thread_pool.hpp>
#include <boost/asio/executor_work_guard.hpp>
#include <thread>
#include "Context.h"

namespace asionet
{

class WorkerPool
{
public:
	WorkerPool(asionet::Context & context, std::size_t numWorkers)
		: context(context)
		  , workGuard(boost::asio::make_work_guard<asionet::Context>(context))
	{
		for (int i = 0; i < numWorkers; ++i)
			workers.push_back(std::make_unique<std::thread>([&] { context.run(); }));
	}

	~WorkerPool()
	{
		stop();
		join();
	}

	void stop()
	{
		context.stop();
	}

	void join()
	{
		for (auto & worker : workers)
		{
			if (worker->joinable())
				worker->join();
		}
	}

private:
	asionet::Context & context;
	std::vector<std::unique_ptr<std::thread>> workers;
	boost::asio::executor_work_guard<asionet::Context::executor_type> workGuard;
};

}

#endif //ASIONET_WORKERPOOL_H
