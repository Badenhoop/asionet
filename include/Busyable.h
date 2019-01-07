//
// Created by philipp on 18.01.18.
//

#ifndef NETWORKINGLIB_BUSYABLE_H
#define NETWORKINGLIB_BUSYABLE_H

#include <mutex>
#include <atomic>
#include <thread>
#include "Error.h"

namespace asionet
{

class Busyable
{
public:
    friend class BusyLock;

    /**
     * @return true if busy, false if not busy
     * @attention Returning false does not guarantee that a subsequent
     *             BusyLock instantiation with this object won't throw!
     */
    bool isBusy() const noexcept
    { return busy; }

private:
    std::mutex busyMutex;
    std::atomic<std::thread::id> busyThreadId{std::thread::id{}};
    std::atomic<bool> busy{false};
};

class BusyLock
{
public:
    explicit BusyLock(Busyable & busyable)
	    : busyable(&busyable)
    {
	    // Its undefined behavior to try to lock a mutex while having it acquired already
	    // so we check the thread's ids for equality.
	    auto thisThreadId = std::this_thread::get_id();
	    if (thisThreadId == busyable.busyThreadId.load() ||
	        !busyable.busyMutex.try_lock())
		    throw std::runtime_error{"busy"};

	    owns = true;
	    busyable.busy = true;
	    busyable.busyThreadId = thisThreadId;
    }

    ~BusyLock()
    {
	    if (owns)
		    unlock();
    }

    BusyLock(const BusyLock &) = delete;

    BusyLock & operator=(const BusyLock &) = delete;

    BusyLock(BusyLock && other) noexcept
	    : busyable(other.busyable)
	      , owns(other.owns.load())
    {
	    other.busyable = nullptr;
	    other.owns = false;
    }

    BusyLock & operator=(BusyLock && other) noexcept
    {
	    if (owns)
		    unlock();

	    busyable = other.busyable;
	    owns = other.owns.load();

	    other.busyable = nullptr;
	    other.owns = false;
    }

    void unlock()
    {
	    busyable->busy = false;
	    busyable->busyThreadId = std::thread::id{};
	    busyable->busyMutex.unlock();
	    owns = false;
    }

protected:
    Busyable * busyable{nullptr};
    std::atomic<bool> owns{false};
};

}


#endif //NETWORKINGLIB_BUSYABLE_H
