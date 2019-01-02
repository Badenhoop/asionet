//
// Created by philipp on 02.01.19.
//

#ifndef ASIONET_WORKER_H
#define ASIONET_WORKER_H

#include <thread>
#include <boost/asio/executor_work_guard.hpp>
#include "Context.h"

namespace asionet
{

class Worker
{
public:
	explicit Worker(asionet::Context & context)
		: context(context)
	{
		thread = std::thread(
			[this]
			{
				auto workGuard = boost::asio::make_work_guard<asionet::Context>(this->context);
				this->context.run();
			});
	}

	~Worker()
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
		if (thread.joinable())
			thread.join();
	}

private:
	asionet::Context & context;
	std::thread thread;
};

}

#endif //ASIONET_WORKER_H
