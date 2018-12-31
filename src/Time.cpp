//
// Created by philipp on 30.12.17.
//

#include "../include/Time.h"

namespace asionet
{
namespace time
{

time::TimePoint now() noexcept
{
    return std::chrono::steady_clock::now();
}

}
}