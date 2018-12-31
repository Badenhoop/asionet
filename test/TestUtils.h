//
// Created by philipp on 30.12.17.
//

#ifndef NETWORKINGLIB_TESTUTILS_H_H
#define NETWORKINGLIB_TESTUTILS_H_H

#include <chrono>

namespace std
{

template<
    class Rep,
    class Period,
    class = std::enable_if_t<
        std::chrono::duration<Rep, Period>::min() < std::chrono::duration<Rep, Period>::zero()>>
constexpr std::chrono::duration<Rep, Period> abs(std::chrono::duration<Rep, Period> d)
{
    return d >= d.zero() ? d : -d;
}

}


#endif //NETWORKINGLIB_TESTUTILS_H_H
