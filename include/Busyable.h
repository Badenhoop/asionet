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
    bool isBusy() const noexcept;

private:
    std::mutex busyMutex;
    std::atomic<std::thread::id> busyThreadId{std::thread::id{}};
    std::atomic<bool> busy{false};
};

class BusyLock
{
public:
    explicit BusyLock(Busyable & busyable);

    ~BusyLock();

    BusyLock(const BusyLock &) = delete;

    BusyLock & operator=(const BusyLock &) = delete;

    BusyLock(BusyLock && other) noexcept;

    BusyLock & operator=(BusyLock && other) noexcept;

    void unlock();

protected:
    Busyable * busyable{nullptr};
    std::atomic<bool> owns{false};
};

}


#endif //NETWORKINGLIB_BUSYABLE_H
