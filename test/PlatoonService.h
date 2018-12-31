//
// Created by philipp on 01.12.17.
//

#ifndef PROTOCOL_NETWORKSERVICES_H
#define PROTOCOL_NETWORKSERVICES_H

#include "PlatoonMessage.h"

namespace protocol
{
class PlatoonService
{
public:
    using RequestMessage = PlatoonMessage;
    using ResponseMessage = PlatoonMessage;
};
}

#endif //PROTOCOL_NETWORKSERVICES_H
