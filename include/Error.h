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

namespace codes
{
constexpr ErrorCode SUCCESS = ErrorCode{0};
constexpr ErrorCode FAILED_OPERATION = ErrorCode{1};
constexpr ErrorCode ABORTED = ErrorCode{2};
constexpr ErrorCode BUSY = ErrorCode{3};
constexpr ErrorCode ENCODING = ErrorCode{4};
constexpr ErrorCode DECODING = ErrorCode{5};
}

class Error : std::runtime_error
{
public:
    Error(const std::string & msg, ErrorCode code)
        : runtime_error(msg), code(code)
    {}

    ErrorCode getErrorCode() const noexcept
    { return code; }

protected:
    ErrorCode code;
};

class FailedOperation : public Error
{
public:
    FailedOperation()
        : Error("networking error: failed operation", codes::FAILED_OPERATION)
    {}
};

class Aborted : public Error
{
public:
    Aborted()
        : Error("networking error: aborted", codes::ABORTED)
    {}
};

class Busy : public Error
{
public:
    Busy()
        : Error("networking error: busy", codes::BUSY)
    {}
};

class Encoding : public Error
{
public:
    Encoding()
        : Error("networking error: encoding", codes::ENCODING)
    {}
};

class Decoding : public Error
{
public:
    Decoding()
        : Error("networking error: decoding", codes::DECODING)
    {}
};

}
}

#endif //NETWORKINGLIB_EXCEPTION_H
