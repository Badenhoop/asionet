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
#ifndef ASIONET_UTILS_H
#define ASIONET_UTILS_H

#include <functional>

namespace asionet
{
namespace utils
{

using Condition = std::function<bool()>;

template<std::size_t numBytes, typename Int>
inline void toBigEndian(std::uint8_t * dest, Int src)
{
	std::size_t bitsToShift = numBytes * 8;
	for (std::size_t i = 0; i < numBytes; i++)
	{
		bitsToShift -= 8;
		dest[i] = (std::uint8_t) ((src >> bitsToShift) & 0x000000ff);
	}
};

template<std::size_t numBytes, typename Int>
inline Int fromBigEndian(const std::uint8_t * bytes)
{
	Int result = 0;
	std::size_t bitsToShift = numBytes * 8;
	for (std::size_t i = 0; i < numBytes; i++)
	{
		bitsToShift -= 8;
		result += ((Int) bytes[i]) << bitsToShift;
	}
	return result;
}

}
}

#endif //ASIONET_UTILS_H
