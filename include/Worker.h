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
			[this]()
			{
				auto workGuard = boost::asio::make_work_guard<asionet::Context>(this->context);
				while (true)
				{
					try
					{
						this->context.run();
						// run() exited normally.
						break;
					}
					catch (...)
					{
						// Ignore exceptions raised by handlers.
					}
				}
			});
	}

	~Worker()
	{
		context.stop();
		thread.join();
	}

private:
	asionet::Context & context;
	std::thread thread;
};

}

#endif //ASIONET_WORKER_H
