//
// Created by philipp on 02.01.19.
//

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
