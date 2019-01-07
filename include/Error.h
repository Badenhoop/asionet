//
// Created by philipp on 02.01.18.
//

#ifndef NETWORKINGLIB_EXCEPTION_H
#define NETWORKINGLIB_EXCEPTION_H

#include <stdexcept>
#include <boost/system/error_code.hpp>
#include <boost/asio/error.hpp>

namespace asionet
{
namespace error
{

using ErrorCode = int;

struct Error
{
	ErrorCode asionetCode{0};
	boost::system::error_code boostCode{};

	boost::system::error_code getBoostCode() const noexcept
	{ return boostCode; }

	explicit operator bool() const noexcept
	{ return asionetCode != 0; }

	friend bool operator==(const Error & lhs, const Error & rhs) noexcept
	{ return lhs.asionetCode == rhs.asionetCode; }

	friend bool operator!=(const Error & lhs, const Error & rhs) noexcept
	{ return !(lhs == rhs); }
};

namespace codes { ErrorCode success{0}; }
const Error success{codes::success};

namespace codes { ErrorCode failedOperation{1}; }
const Error failedOperation{codes::failedOperation};

namespace codes { ErrorCode aborted{2}; }
const Error aborted{codes::aborted};

namespace codes { ErrorCode encoding{3}; }
const Error encoding{codes::encoding};

namespace codes { ErrorCode decoding{4}; }
const Error decoding{codes::decoding};

namespace codes { ErrorCode invalidFrame{5}; }
const Error invalidFrame{codes::invalidFrame};

}
}

#endif //NETWORKINGLIB_EXCEPTION_H
