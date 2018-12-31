//
// Created by philipp on 28.12.17.
//

#include "../include/Timer.h"
#include "../include/Error.h"

namespace asionet
{
namespace time
{

Timer::Timer(PrivateTag, Networking & net)
    : timer(net.getIoService())
{}

Timer::Ptr Timer::create(Networking & net)
{
    return std::make_shared<Timer>(PrivateTag{}, net);
}

void Timer::startTimeout(const time::Duration & duration, const TimeoutHandler & handler)
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

void Timer::startPeriodicTimeout(const time::Duration & interval, const TimeoutHandler & handler)
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

void Timer::stop()
{
    if (!enabled)
        return;

    enabled = false;
    boost::system::error_code ignoredError;
    timer.cancel(ignoredError);
}

void Timer::nextPeriod(std::shared_ptr<AsyncState> & state)
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

Timer::AsyncState::AsyncState(Timer::Ptr self, const Timer::TimeoutHandler & handler, const time::Duration & duration)
    : busyLock(*self)
      , self(self)
      , handler(handler)
      , duration(duration)
{
}

}
}