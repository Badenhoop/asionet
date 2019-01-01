//
// Created by philipp on 06.12.17.
//

#ifndef PROTOCOL_NETWORKING_H
#define PROTOCOL_NETWORKING_H

#include <boost/asio/io_service.hpp>
#include <thread>
#include <condition_variable>
#include "Busyable.h"

namespace asionet
{

class Networking
{
public:
	using Handler = std::function<void()>;
	using Condition = std::function<bool()>;

    struct WaitCondition
    {
	    explicit WaitCondition(const Condition & condition)
		    : condition(condition)
	    {}

	    std::mutex mutex;
	    std::condition_variable variable;
	    Condition condition;
    };

    Networking()
    {
        work = std::make_unique<boost::asio::io_service::work>(ioService);
        thread = std::thread(
            [this]()
            {
                while (true)
                {
                    try
                    {
                        ioService.run();
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

    ~Networking()
    {
        work.reset();
        ioService.stop();
        thread.join();
    }

    Networking(const Networking & other) = delete;

    Networking & operator=(const Networking & other) = delete;

    Networking(Networking && other) = delete;

    Networking & operator=(Networking && other) = delete;

    void waitUntil(WaitCondition & waitCondition)
    {
        // This one is quite tricky:
        // We want to wait until the condition becomes true.
        // So during our waiting, we have to run ioService. But there are two cases to consider:
        // We were called from an ioService handler and therefor from the ioService-thread:
        //      In this case we must invoke ioService.run_one() to ensure that further handlers can be invoked.
        // Else we were not called from the ioService-thread:
        //      Since we must not call ioService.run_one() from a different thread (since we assume ioService.run() permanently
        //      runs already on the ioService-thread, we just wait until the error changed "magically".
        if (std::this_thread::get_id() == thread.get_id())
        {
            while (!ioService.stopped() && !waitCondition.condition())
                ioService.run_one();
        }
        else
        {
	        std::unique_lock<std::mutex> lock{waitCondition.mutex};
	        while (!ioService.stopped() && !waitCondition.condition())
	        {
		        waitCondition.variable.wait(lock);
	        }
        }
    }

    void callLater(const Handler & handler)
    {
        ioService.post(handler);
    }

    boost::asio::io_service & getIoService() noexcept
    { return ioService; }

private:
    boost::asio::io_service ioService;
    std::unique_ptr<boost::asio::io_service::work> work;
    std::thread thread;
};

}

#endif //PROTOCOL_NETWORKING_H
