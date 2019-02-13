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
#ifndef ASIONET_TEST_H
#define ASIONET_TEST_H

#include <string>
#include "../include/asionet/Message.h"

namespace asionet
{
namespace test
{

class StringService
{
public:
    using RequestMessage = std::string;
    using ResponseMessage = std::string;
};

class NonCopyableMessage
{
public:
    NonCopyableMessage() = default;
    NonCopyableMessage(const NonCopyableMessage &) = delete;
    NonCopyableMessage & operator=(const NonCopyableMessage &) = delete;
    NonCopyableMessage(NonCopyableMessage &&) = delete;
    NonCopyableMessage & operator=(NonCopyableMessage &&) = delete;
};

}
}

namespace asionet
{
namespace message
{

template<>
struct Encoder<asionet::test::NonCopyableMessage>
{
    void operator()(const asionet::test::NonCopyableMessage & message, std::string & data) const
    { data = ""; }
};

template<>
struct Decoder<asionet::test::NonCopyableMessage>
{
    template<typename ConstBuffer>
    void operator()(const ConstBuffer & buffer, asionet::test::NonCopyableMessage & message) const
    {}
};

}
}

#endif //ASIONET_TEST_H
