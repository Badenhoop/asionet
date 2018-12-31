//
// Created by philipp on 15.12.17.
//

#include "../include/Networking.h"

namespace asionet
{

Networking::Networking()
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

Networking::~Networking()
{
    work.reset();
    ioService.stop();
    thread.join();
}

void Networking::waitUntil(Condition condition)
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
        while (!ioService.stopped() && !condition())
            ioService.run_one();
    }
    else
    {
        while (!ioService.stopped() && !condition());
    }
}

void Networking::waitWhileBusy(Busyable & busyable)
{
    waitUntil(
        [&]
        { return !busyable.isBusy(); });
}

void Networking::callLater(const Handler & handler)
{
    ioService.post(handler);
}

}