//
// Created by philipp on 28.12.17.
//

#ifndef NETWORKINGLIB_TIMER_H
#define NETWORKINGLIB_TIMER_H

#include "Networking.h"
#include <boost/asio.hpp>
#include <boost/asio/steady_timer.hpp>
#include "Time.h"
#include "Busyable.h"

namespace asionet
{
namespace time
{

class Timer
    : public std::enable_shared_from_this<Timer>
      , public Busyable
{
private:
    struct PrivateTag
    {
    };

public:
    using Ptr = std::shared_ptr<Timer>;
    using TimeoutHandler = std::function<void()>;

    // Objects of this class should always be declared as std::shared_ptr.
    Timer(PrivateTag, Networking & net);

    static Ptr create(Networking & net);

    void startTimeout(const time::Duration & duration, const TimeoutHandler & handler);

    void startPeriodicTimeout(const time::Duration & interval, const TimeoutHandler & handler);

    void stop();

private:
    struct AsyncState
    {
        AsyncState(Ptr self, const TimeoutHandler & handler, const time::Duration & duration);

        BusyLock busyLock;
        Ptr self;
        TimeoutHandler handler;
        time::Duration duration;
    };

    boost::asio::basic_waitable_timer<time::Clock> timer;
    std::atomic<bool> enabled{true};

    void nextPeriod(std::shared_ptr<AsyncState> & state);
};

}
}

#endif //NETWORKINGLIB_TIMER_H
