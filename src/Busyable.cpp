//
// Created by philipp on 21.01.18.
//

#include "../include/NetworkingLib/Busyable.h"

namespace networking
{

bool Busyable::isBusy() const noexcept
{
    return busy;
}

BusyLock::BusyLock(Busyable & busyable) : busyable(&busyable)
{
    // Its undefined behavior to try to lock a mutex while having it acquired already
    // so we check the thread's ids for equality.
    auto thisThreadId = std::this_thread::get_id();
    if (thisThreadId == busyable.busyThreadId.load() ||
        !busyable.busyMutex.try_lock())
        throw error::Busy{};

    owns = true;
    busyable.busy = true;
    busyable.busyThreadId = thisThreadId;
}

BusyLock::~BusyLock()
{
    if (owns)
        unlock();
}

BusyLock::BusyLock(BusyLock && other) noexcept
    : busyable(other.busyable)
      , owns(other.owns.load())
{
    other.busyable = nullptr;
    other.owns = false;
}

BusyLock & BusyLock::operator=(BusyLock && other) noexcept
{
    if (owns)
        unlock();

    busyable = other.busyable;
    owns = other.owns.load();

    other.busyable = nullptr;
    other.owns = false;
}

void BusyLock::unlock()
{
    busyable->busy = false;
    busyable->busyThreadId = std::thread::id{};
    busyable->busyMutex.unlock();
    owns = false;
}

}