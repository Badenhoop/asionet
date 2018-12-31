//
// Created by philipp on 05.02.18.
//

#ifndef PC2CAR_TIMEDVALUE_H
#define PC2CAR_TIMEDVALUE_H

#include "Time.h"
#include <mutex>

namespace networking
{
namespace time
{

template<typename T>
class TimedValue
{
public:
    using Timestamp = networking::time::TimePoint;

    TimedValue()
        : timestamp(Timestamp::min())
    {}

    explicit TimedValue(const T & val)
    { set(val); }

    TimedValue(const T & val, const Timestamp & timestamp)
    { set(val, timestamp); }

    void set(const T & val, Timestamp timestamp)
    {
        this->val = val;
        this->timestamp = timestamp;
    }

    void set(const T & val)
    { set(val, networking::time::now()); }

    void set(const TimedValue & val)
    { set(val.get(), val.getTimestamp()); }

    TimedValue & operator=(const T & val)
    { set(val); }

    T get() const noexcept
    { return val; }

    Timestamp getTimestamp() const noexcept
    { return timestamp; }

    explicit operator T() const noexcept
    { return get(); }

private:
    T val;
    Timestamp timestamp;
};

template<typename T>
class TimedAtomicValue
{
public:
    using Timestamp = networking::time::TimePoint;

    TimedAtomicValue()
        : timestamp(Timestamp::min())
    {}

    explicit TimedAtomicValue(const T & val)
    { set(val); }

    TimedAtomicValue(const T & val, Timestamp timestamp)
    { set(val, timestamp); }

    explicit TimedAtomicValue(const TimedValue<T> val)
    { set(val); }

    void set(const T & val, Timestamp timestamp)
    {
        std::lock_guard <std::mutex> lock{mutex};
        this->val = val;
        this->timestamp = timestamp;
    }

    void set(const T & val)
    { set(val, networking::time::now()); }

    void set(const TimedValue<T> & val)
    { set(val.get(), val.getTimestamp()); }

    TimedAtomicValue & operator=(const T & val)
    { set(val); }

    TimedAtomicValue & operator=(const TimedValue<T> & val)
    { set(val); }

    T get()
    {
        std::lock_guard <std::mutex> lock{mutex};
        return val;
    }

    Timestamp getTimestamp()
    {
        std::lock_guard <std::mutex> lock{mutex};
        return timestamp;
    }

    explicit operator T()
    { return get(); }

    TimedValue<T> getNonAtomicCopy()
    {
        std::lock_guard <std::mutex> lock{mutex};
        return TimedValue<T>{val, timestamp};
    }

private:
    T val;
    Timestamp timestamp;
    std::mutex mutex;
};

}

}

#endif //PC2CAR_TIMEDVALUE_H
