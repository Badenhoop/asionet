//
// Created by philipp on 30.12.17.
//

#ifndef NETWORKINGLIB_TIME_H
#define NETWORKINGLIB_TIME_H

#include <chrono>

namespace asionet
{
namespace time
{

using Clock = std::chrono::system_clock;
using TimePoint = Clock::time_point;
using Duration = Clock::duration;

inline TimePoint now() noexcept
{
	return Clock::now();
}

}
}

#endif //NETWORKINGLIB_TIME_H
