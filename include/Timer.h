//
// Created by philipp on 28.12.17.
//

#ifndef NETWORKINGLIB_TIMER_H
#define NETWORKINGLIB_TIMER_H

#include <boost/asio.hpp>
#include <boost/asio/steady_timer.hpp>
#include "Time.h"
#include "Busyable.h"
#include "Context.h"

namespace asionet
{

class Timer : public Busyable
{
public:
    using TimeoutHandler = std::function<void()>;

    // Objects of this class should always be declared as std::shared_ptr.
    explicit Timer(asionet::Context & context)
        : timer(context)
    {}

    void startTimeout(const time::Duration & duration, const TimeoutHandler & handler)
    {
        auto state = std::make_shared<AsyncState>(*this, handler, duration);

        enabled = true;

        timer.expires_from_now(duration);
        timer.async_wait(
            [this, state](const boost::system::error_code & error) mutable
            {
                if (error || !enabled)
                    return;

                state->busyLock.unlock();
                state->handler();
            });
    }

    void startPeriodicTimeout(const time::Duration & interval, const TimeoutHandler & handler)
    {
        auto state = std::make_shared<AsyncState>(*this, handler, interval);

        enabled = true;

        timer.expires_from_now(interval);
        timer.async_wait(
            [this, state = std::move(state)](const boost::system::error_code & error) mutable
            {
                if (error || !enabled)
                    return;

                state->handler();
                nextPeriod(state);
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
        AsyncState(Timer & timer, const TimeoutHandler & handler, const time::Duration & duration)
            : busyLock(timer)
              , handler(handler)
              , duration(duration)
        {}

        BusyLock busyLock;
        TimeoutHandler handler;
        time::Duration duration;
    };

    boost::asio::basic_waitable_timer<time::Clock> timer;
    std::atomic<bool> enabled{true};

    void nextPeriod(std::shared_ptr<AsyncState> & state)
    {
        if (!enabled)
            return;

        timer.expires_at(timer.expires_at() + state->duration);
        timer.async_wait(
            [this, state](const boost::system::error_code & error) mutable
            {
                if (error || !enabled)
                    return;

                state->handler();
                nextPeriod(state);
            });
    }
};

}

#endif //NETWORKINGLIB_TIMER_H
