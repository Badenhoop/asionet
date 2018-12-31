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
    Timer(PrivateTag, Networking & net)
        : timer(net.getIoService())
    {}

    static Ptr create(Networking & net)
    {
        return std::make_shared<Timer>(PrivateTag{}, net);
    }

    void startTimeout(const time::Duration & duration, const TimeoutHandler & handler)
    {
        auto self = shared_from_this();
        auto state = std::make_shared<AsyncState>(self, handler, duration);

        enabled = true;

        timer.expires_from_now(duration);
        timer.async_wait(
            [state](const boost::system::error_code & error) mutable
            {
                if (error || !state->self->enabled)
                    return;

                state->busyLock.unlock();
                state->handler();
            });
    }

    void startPeriodicTimeout(const time::Duration & interval, const TimeoutHandler & handler)
    {
        auto self = shared_from_this();
        auto state = std::make_shared<AsyncState>(self, handler, interval);

        enabled = true;

        timer.expires_from_now(interval);
        timer.async_wait(
            [state = std::move(state)](const boost::system::error_code & error) mutable
            {
                if (error || !state->self->enabled)
                    return;

                state->handler();
                state->self->nextPeriod(state);
            });
    }

    void stop()
    {
        if (!enabled)
            return;

        enabled = false;
        boost::system::error_code ignoredError;
        timer.cancel(ignoredError);
    }

private:
    struct AsyncState
    {
        AsyncState(Ptr self, const TimeoutHandler & handler, const time::Duration & duration)
            : busyLock(*self)
              , self(self)
              , handler(handler)
              , duration(duration)
        {}

        BusyLock busyLock;
        Ptr self;
        TimeoutHandler handler;
        time::Duration duration;
    };

    boost::asio::basic_waitable_timer<time::Clock> timer;
    std::atomic<bool> enabled{true};

    void nextPeriod(std::shared_ptr<AsyncState> & state)
    {
        if (!state->self->enabled)
            return;

        state->self->timer.expires_at(state->self->timer.expires_at() + state->duration);
        state->self->timer.async_wait(
            [state](const boost::system::error_code & error) mutable
            {
                if (error || !state->self->enabled)
                    return;

                state->handler();
                state->self->nextPeriod(state);
            });
    }
};

}
}

#endif //NETWORKINGLIB_TIMER_H
