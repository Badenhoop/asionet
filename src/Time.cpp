//
// Created by philipp on 30.12.17.
//

#include "../include/NetworkingLib/Time.h"

namespace networking
{
namespace time
{

time::TimePoint now() noexcept
{
    return std::chrono::steady_clock::now();
}

}
}