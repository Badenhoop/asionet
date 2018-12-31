//
// Created by philipp on 06.12.17.
//

#ifndef PROTOCOL_NETWORKING_H
#define PROTOCOL_NETWORKING_H

#include <boost/asio/io_service.hpp>
#include <thread>
#include "Busyable.h"

namespace networking
{

class Networking
{
public:
    using Condition = std::function<bool()>;
    using Handler = std::function<void()>;

    Networking();

    ~Networking();

    Networking(const Networking & other) = delete;

    Networking & operator=(const Networking & other) = delete;

    Networking(Networking && other) = delete;

    Networking & operator=(Networking && other) = delete;

    void waitUntil(Condition condition);

    void waitWhileBusy(Busyable & busyable);

    void callLater(const Handler & handler);

    boost::asio::io_service & getIoService() noexcept
    { return ioService; }

private:
    boost::asio::io_service ioService;
    std::unique_ptr<boost::asio::io_service::work> work;
    std::thread thread;
};

}

#endif //PROTOCOL_NETWORKING_H
