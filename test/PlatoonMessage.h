//
// Created by philipp on 01.12.17.
//

#ifndef PROTOCOL_PLATOONCREATEMESSAGE_H
#define PROTOCOL_PLATOONCREATEMESSAGE_H

#include <cstdint>
#include <vector>
#include "../include/Message.h"

namespace protocol
{

using VehicleId = std::uint32_t;
using MessageType = std::uint8_t;
using PlatoonId = std::uint32_t;

namespace messageTypes
{
constexpr MessageType LEADER_REQUEST = 0x01;
constexpr MessageType FOLLOWER_REQUEST = 0x02;
constexpr MessageType ACCEPT_RESPONSE = 0x03;
constexpr MessageType REJECT_RESPONSE = 0x04;
}

class PlatoonMessage
{
public:
    PlatoonMessage() = default;

    PlatoonMessage(VehicleId vehicleId, MessageType messageType, PlatoonId platoonId)
        : vehicleId(vehicleId)
          , messageType(messageType)
          , platoonId(platoonId)
    {}

    static PlatoonMessage followerRequest(VehicleId vehicleId)
    { return PlatoonMessage(vehicleId, messageTypes::FOLLOWER_REQUEST, 0); }

    static PlatoonMessage leaderRequest(VehicleId vehicleId, PlatoonId platoonId)
    { return PlatoonMessage(vehicleId, messageTypes::LEADER_REQUEST, platoonId); }

    static PlatoonMessage rejectResponse(VehicleId vehicleId)
    { return PlatoonMessage(vehicleId, messageTypes::REJECT_RESPONSE, 0); }

    static PlatoonMessage acceptResponse(VehicleId vehicleId, PlatoonId platoonId)
    { return PlatoonMessage(vehicleId, messageTypes::ACCEPT_RESPONSE, platoonId); }

    VehicleId getVehicleId() const
    { return vehicleId; }

    MessageType getMessageType() const
    { return messageType; }

    PlatoonId getPlatoonId() const
    { return platoonId; }

    std::shared_ptr<PlatoonMessage> toPtr()
    { return std::make_shared<PlatoonMessage>(vehicleId, messageType, platoonId); }

private:
    VehicleId vehicleId;
    MessageType messageType;
    PlatoonId platoonId;
};

}

namespace asionet
{
namespace message
{

template<>
struct Encoder<protocol::PlatoonMessage>
{
    void operator()(const protocol::PlatoonMessage & message, std::string & data) const
    {
        using namespace protocol;

        data = std::string(9, '\0');

        auto vehicleId = message.getVehicleId();
        auto messageType = message.getMessageType();
        auto platoonId = message.getPlatoonId();

        data[0] = (std::uint8_t) (vehicleId & 0x000000ff);
        data[1] = (std::uint8_t) ((vehicleId & 0x0000ff00) >> 8);
        data[2] = (std::uint8_t) ((vehicleId & 0x00ff0000) >> 16);
        data[3] = (std::uint8_t) ((vehicleId & 0xff000000) >> 24);
        data[4] = (std::uint8_t) (messageType);
        data[5] = (std::uint8_t) (platoonId & 0x000000ff);
        data[6] = (std::uint8_t) ((platoonId & 0x0000ff00) >> 8);
        data[7] = (std::uint8_t) ((platoonId & 0x00ff0000) >> 16);
        data[8] = (std::uint8_t) ((platoonId & 0xff000000) >> 24);
    }
};

template<>
struct Decoder<protocol::PlatoonMessage>
{
    std::shared_ptr<protocol::PlatoonMessage> operator()(const std::string & data) const
    {
        using namespace protocol;

        auto size = data.size();

        VehicleId vehicleId{0};
        MessageType messageType{0};
        PlatoonId platoonId{0};

        vehicleId += ((VehicleId) data[0]);
        vehicleId += ((VehicleId) data[1]) << 8;
        vehicleId += ((VehicleId) data[2]) << 16;
        vehicleId += ((VehicleId) data[3]) << 24;
        messageType += (MessageType) data[4];
        platoonId += ((PlatoonId) data[5]);
        platoonId += ((PlatoonId) data[6]) << 8;
        platoonId += ((PlatoonId) data[7]) << 16;
        platoonId += ((PlatoonId) data[8]) << 24;

        return std::make_shared<PlatoonMessage>(vehicleId, messageType, platoonId);
    }
};

}
}

#endif //PROTOCOL_PLATOONCREATEMESSAGE_H
