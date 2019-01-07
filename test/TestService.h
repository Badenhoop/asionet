//
// Created by philipp on 01.12.17.
//

#ifndef PROTOCOL_NETWORKSERVICES_H
#define PROTOCOL_NETWORKSERVICES_H

#include "TestMessage.h"

namespace protocol
{
class TestService
{
public:
    using RequestMessage = TestMessage;
    using ResponseMessage = TestMessage;
};
}

#endif //PROTOCOL_NETWORKSERVICES_H
