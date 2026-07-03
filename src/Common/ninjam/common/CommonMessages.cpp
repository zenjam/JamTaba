#include "CommonMessages.h"

using ninjam::INetworkMessage;
using ninjam::common::KeepAliveMessage;

//+++++++++++++++++++++

KeepAliveMessage::KeepAliveMessage()
    : CommonMessage(MessageType::KeepAlive)
{

}

quint32 KeepAliveMessage::getSerializePayload() const
{
    return 0;
}

bool KeepAliveMessage::unserializeFrom(NinjamInputDataStream& stream)
{
    return true;
}

void KeepAliveMessage::printDebug(QDebug &dbg) const
{
    dbg << "KeepAliveMessage{}"
        << '\n';
}
