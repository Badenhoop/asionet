//
// Created by philipp on 04.01.19.
//

#ifndef ASIONET_MONITOR_H
#define ASIONET_MONITOR_H

#include <mutex>

namespace asionet
{
namespace utils
{

template<class T>
class Monitor
{
private:
	mutable T t;
	mutable std::mutex mutex;

public:
	using Type = T;

	Monitor() = default;

	explicit Monitor(T t) : t(std::move(t))
	{}

	template<typename F>
	auto operator()(F f) const -> decltype(f(t))
	{
		std::lock_guard <std::mutex> lock{mutex};
		return f(t);
	}
};

}
}

#endif //ASIONET_MONITOR_H
