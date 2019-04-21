/*
 * The MIT License
 * 
 * Copyright (c) 2019 Philipp Badenhoop
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#ifndef ASIONET_ERROR_H
#define ASIONET_ERROR_H

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

namespace codes { constexpr ErrorCode success{0}; }
const Error success{codes::success};

namespace codes { constexpr ErrorCode failedOperation{1}; }
const Error failedOperation{codes::failedOperation};

namespace codes { constexpr ErrorCode aborted{2}; }
const Error aborted{codes::aborted};

namespace codes { constexpr ErrorCode encoding{3}; }
const Error encoding{codes::encoding};

namespace codes { constexpr ErrorCode decoding{4}; }
const Error decoding{codes::decoding};

namespace codes { constexpr ErrorCode invalidFrame{5}; }
const Error invalidFrame{codes::invalidFrame};

}
}

#endif //ASIONET_ERROR_H
