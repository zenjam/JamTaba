#include "ClientMessages.h"
#include "ninjam/client/User.h"
#include "ninjam/client/UserChannel.h"
#include "ninjam/Ninjam.h"
#include "ninjam/client/Types.h"

#include <QCryptographicHash>
#include <QIODevice>
#include <QDebug>
#include <QDataStream>
#include <QString>

using ninjam::client::ClientAuthUserMessage;
using ninjam::client::UploadIntervalWrite;
using ninjam::client::ClientSetChannel;
using ninjam::client::ClientSetUserMask;
using ninjam::client::UploadIntervalBegin;
using ninjam::client::ClientToServerChatMessage;
using ninjam::client::UserChannel;
using ninjam::MessageType;

//++++++++++++++++++++++++++++++++++++++=
//class ClientAuthUser
/*
     //message type 0x80

     Offset Type        Field
     0x0    uint8_t[20] Password Hash (binary hash value)
     0x14   ...         Username (NUL-terminated)
     x+0x0  uint32_t    Client Capabilities
     x+0x4  uint32_t    Client Version

     The Password Hash field is calculated by SHA1(SHA1(username + ":" + password) + challenge).
     If the user acknowledged a license agreement from the server then Client Capabilities bit 0 is set.
     The server responds with Server Auth Reply.

    //message lenght = 20 bytes password hash + user name lengh + 4 bytes client capabilites + 4 bytes client version
*/

bool ClientAuthUserMessage::processReadedData(const QString& password) {
    clientCapabilites = 1;
    if (password.isNull() || password.isEmpty()) {
        userName = "anonymous:" + userName;
    }
    QCryptographicHash sha1(QCryptographicHash::Sha1);
    sha1.addData(userName.toStdString().c_str(), userName.size());
    sha1.addData(":", 1);
    sha1.addData(password.toStdString().c_str(), password.size());
    QByteArray passHash = sha1.result();
    sha1.reset();
    sha1.addData(passHash);
    sha1.addData(challenge.constData(), 8);
    passwordHash = sha1.result();
    return true;
}

ClientAuthUserMessage::ClientAuthUserMessage()
    : ClientMessage(MessageType::ClientAuthUser),
      challenge(8, Qt::Uninitialized)
{

}

ClientAuthUserMessage::ClientAuthUserMessage(const QString &userName, const QByteArray &challenge, quint32 protocolVersion, const QString &password)
    : ClientMessage(MessageType::ClientAuthUser),
      userName(userName),
      clientCapabilites(1),
      protocolVersion(protocolVersion),
      challenge(challenge)
{
    processReadedData(password);
}

quint32 ClientAuthUserMessage::getSerializePayload() const
{
    return NinjamOutputDataStream::getByteArrayPayload(passwordHash) +
            NinjamOutputDataStream::getUtf8StringPayload(userName) +
            NinjamOutputDataStream::getPayload<quint32>() +
            NinjamOutputDataStream::getPayload<quint32>();
}

bool ClientAuthUserMessage::serializeTo(NinjamOutputDataStream& stream) const
{
    return (ClientMessage::serializeTo(stream) &&
            stream.writeByteArray(passwordHash) &&
            stream.writeUtf8String(userName) &&
            stream.write<quint32>(clientCapabilites) &&
            stream.write<quint32>(protocolVersion));
}

bool ClientAuthUserMessage::unserializeFrom(NinjamInputDataStream& stream)
{
    QString password;
    return (stream.readAciiStringFixed(password, 20) &&
            stream.readUtf8String(userName) &&
            stream.readByteArray(challenge) &&
            stream.read<quint32>(clientCapabilites) &&
            stream.read<quint32>(protocolVersion) &&
            processReadedData(password));
}

void ClientAuthUserMessage::printDebug(QDebug &dbg) const
{
    dbg << "SEND ClientAuthUserMessage{  userName:"
        << userName
        << " challenge:"
        << challenge
        << "}"
        << '\n';
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

ClientSetChannel::ClientSetChannel() :
    ClientMessage(MessageType::ClientSetChannel)
{
}

ClientSetChannel::ClientSetChannel(const QList<ninjam::client::ChannelMetadata> &channels) :
    ClientSetChannel()
{
    for (const auto& channel : channels) {
        quint8 flags = channel.voiceChatActivated ? 2 : 0;  //Possible values: 0 - ninjam interval based , 2 - voice chat, 4 - session mode
        addChannel(channel.name, flags, true);
    }
}

quint32 ClientSetChannel::getSerializePayload() const {
    quint32 payload = NinjamOutputDataStream::getPayload<quint16>();
    for (const UserChannel &channel : channels) {
        payload += NinjamOutputDataStream::getUtf8StringPayload(channel.getName());
        payload += NinjamOutputDataStream::getPayload<quint16>();
        payload += NinjamOutputDataStream::getPayload<quint8>();
        payload += NinjamOutputDataStream::getPayload<quint8>();
    }
    return payload;
}

bool ClientSetChannel::serializeTo(NinjamOutputDataStream& stream) const
{
    if (ClientMessage::serializeTo(stream) &&
        stream.write<quint16>(4)) { // parameter size (4 bytes - volume (2 bytes) + pan (1 byte) + flags (1 byte))
        for (const UserChannel &channel : channels) {
            if (stream.writeUtf8String(channel.getName()) &&
                stream.write<quint16>(channel.getVolume()) &&
                stream.write<quint8>(channel.getPan()) &&
                stream.write<quint8>(static_cast<quint8>(channel.getFlags()))) {
                // write channel info done
            } else {
                return false;
            }
        }
        return true;
    }
    return false;
}

bool ClientSetChannel::unserializeFrom(NinjamInputDataStream& stream)
{
    if (stream.getRemainingPayload() == 0) {
        channels.clear();
        return true; // no channels
    }
    quint16 channelParameterSize; // ??
    if (!stream.read<quint16>(channelParameterSize)) {
        return false;
    }
    channels.clear();
    while (stream.getRemainingPayload() != 0) {
        QString channelName;
        quint16 volume;
        quint8 pan;
        quint8 flags;
        if (stream.readUtf8String(channelName) &&
            stream.read<quint16>(volume) &&
            stream.read<quint8>(pan) &&
            stream.read<quint8>(flags)) {
            bool active = true; // flags == 0;
            addChannel(channelName, flags, active);
        } else {
            return false;
        }
    }
    return true;
}

void ClientSetChannel::printDebug(QDebug &dbg) const
{
    dbg << "SEND ClientSetChannel{ channels="
        << channels.size()
        << '}'
        << '\n';
}

void ClientSetChannel::addChannel(const QString &channelName, quint8 flags, bool active)
{
    channels.append(UserChannel(channelName, channels.size(), static_cast<UserChannel::Flags>(flags), active));
}

//+++++++++++++++++

ClientSetUserMask::ClientSetUserMask() :
    ClientMessage(MessageType::ClientSetUserMask)
{

}

ClientSetUserMask::ClientSetUserMask(const QString &userFullName, quint32 channelsMask)
    : ClientMessage(MessageType::ClientSetUserMask),
      userName(userFullName),
      channelsMask(channelsMask)
{

}

quint32 ClientSetUserMask::getSerializePayload() const {
    return NinjamOutputDataStream::getUtf8StringPayload(userName) +
            NinjamOutputDataStream::getPayload<quint32>();
}

bool ClientSetUserMask::serializeTo(NinjamOutputDataStream& stream) const
{
    return (ClientMessage::serializeTo(stream) &&
            stream.writeUtf8String(userName) &&
            stream.write<quint32>(channelsMask));
}

bool ClientSetUserMask::unserializeFrom(NinjamInputDataStream& stream)
{
    return (stream.readUtf8String(userName) &&
            stream.read<quint32>(channelsMask));
}

void ClientSetUserMask::printDebug(QDebug &dbg) const
{
    dbg << "SEND ClientSetUserMask{ userName="
        << userName
        << " flag="
        << channelsMask
        << '}'
        << '\n';
}

//+++++++++++++++++++++++++++++
ClientToServerChatMessage::ClientToServerChatMessage()
    : ClientMessage(MessageType::ChatMessage)
{
    //Q_ASSERT(arguments.size() == 4);
}


ClientToServerChatMessage::ClientToServerChatMessage(const QString &command, const QString &arg1, const QString &arg2, const QString &arg3, const QString &arg4) :
    ClientMessage(MessageType::ChatMessage),
    command(command)
{
    arguments.append(arg1);
    arguments.append(arg2);
    arguments.append(arg3);
    arguments.append(arg4);

    Q_ASSERT(arguments.size() == 4);
}

ClientToServerChatMessage ClientToServerChatMessage::buildPublicMessage(const QString &message)
{
    return ClientToServerChatMessage("MSG", message, QString(), QString(), QString());
}

ClientToServerChatMessage ClientToServerChatMessage::buildPrivateMessage(const QString &message, const QString &destionationUserName)
{
    QString sanitizedMessage(message);
    sanitizedMessage.replace(QString("/msg "), "");

    return ClientToServerChatMessage("PRIVMSG", destionationUserName, sanitizedMessage, QString(), QString());
}

ClientToServerChatMessage ClientToServerChatMessage::buildAdminMessage(const QString &message)
{
    QString msg = message.right(message.size() - 1); // remove the first char (/)
    return ClientToServerChatMessage("ADMIN", msg, QString(), QString(), QString());
}

bool ClientToServerChatMessage::isBpiVoteMessage() const
{
    if (!isPublicMessage())
        return false;

    QString voteCommand = arguments.at(0);
    return voteCommand.startsWith("!vote bpi ");
}

quint16 ClientToServerChatMessage::extractVoteValue(const QString &string)
{
    auto parts = string.split(" ");
    if (parts.size() == 3) {
        return static_cast<quint16>(parts.at(2).toInt());
    }

    return 0;
}


quint16 ClientToServerChatMessage::extractBpmVoteValue() const
{
    if (isBpmVoteMessage()) {
        if (!arguments.empty()) {
            return extractVoteValue(arguments.at(0));
        }
    }

    return 0;
}

quint16 ClientToServerChatMessage::extractBpiVoteValue() const
{
    if (isBpiVoteMessage()) {
        if (!arguments.empty()) {
            return extractVoteValue(arguments.at(0));
        }
    }

    return 0;
}

bool ClientToServerChatMessage::isBpmVoteMessage() const
{
    if (!isPublicMessage())
        return false;

    const QString& voteCommand = arguments.at(0);
    return voteCommand.startsWith("!vote bpm ");
}

bool ClientToServerChatMessage::isPublicMessage() const
{
    return command == "MSG";
}

bool ClientToServerChatMessage::isPrivateMessage() const
{
    return command == "PRIVMSG";
}

bool ClientToServerChatMessage::isAdminMessage() const
{
    return command == "ADMIN";
}

quint32 ClientToServerChatMessage::getSerializePayload() const
{
    quint32 payload = NinjamOutputDataStream::getUtf8StringPayload(command);
    for (const auto &arg : arguments) {
        payload += NinjamOutputDataStream::getUtf8StringPayload(arg);
    }
    return payload;
}

bool ClientToServerChatMessage::serializeTo(NinjamOutputDataStream& stream) const
{
    if (ClientMessage::serializeTo(stream) &&
            stream.writeUtf8String(command)) {
        for (const auto &arg : arguments) {
            if (!stream.writeUtf8String(arg)) {
                return false;
            }
        }
        return true;
    }
    return false;
}

bool ClientToServerChatMessage::unserializeFrom(NinjamInputDataStream& stream)
{
    QStringList strings;
    if (stream.readUtf8Strings(strings, stream.getRemainingPayload()) &&
            strings.size() > 1) {
        command = strings[0];
        arguments.clear();
        arguments.append(strings[1]);
        arguments.append((strings.size() >= 3) ? strings[2] : QString());
        arguments.append((strings.size() >= 4) ? strings[3] : QString());
        arguments.append((strings.size() >= 5) ? strings[4] : QString());
        return true;
    }
    return false;
}

void ClientToServerChatMessage::printDebug(QDebug &dbg) const
{
    dbg << "SEND ChatMessage{"
        << "command="
        << command
        << " arg1=" << arguments.at(0)
        << " arg2=" << arguments.at(1)
        << " arg3=" << arguments.at(2)
        << " arg4=" << arguments.at(3)
        << '}'
        << '\n';
}

//+++++++++++++++++++++++++

UploadIntervalBegin::UploadIntervalBegin() :
    ClientMessage(MessageType::UploadIntervalBegin)
{

}

UploadIntervalBegin::UploadIntervalBegin(const MessageGuid &GUID, quint8 channelIndex, bool isAudioInterval) :
    ClientMessage(MessageType::UploadIntervalBegin),
    GUID(GUID),
    estimatedSize(0),
    channelIndex(channelIndex)
{
    if (isAudioInterval) {
        this->fourCC[0] = 'O';
        this->fourCC[1] = 'G';
        this->fourCC[2] = 'G';
        this->fourCC[3] = 'v';
    }
    else {
        // JamTaba Video prefix
        this->fourCC[0] = 'J';
        this->fourCC[1] = 'T';
        this->fourCC[2] = 'B';
        this->fourCC[3] = 'v';
    }
}

quint32 UploadIntervalBegin::getSerializePayload() const
{
    return sizeof(GUID) + sizeof(estimatedSize) + sizeof(fourCC) + sizeof(channelIndex);
}

bool UploadIntervalBegin::serializeTo(NinjamOutputDataStream& stream) const
{
    return (ClientMessage::serializeTo(stream) &&
            stream.writeByteArray(GUID) &&
            stream.write<quint32>(estimatedSize) &&
            stream.writeByteArray(fourCC) &&
            stream.write<quint8>(channelIndex));
}

bool UploadIntervalBegin::unserializeFrom(NinjamInputDataStream& stream) {
    return (stream.readByteArray(GUID) &&
            stream.read<quint32>(estimatedSize) &&
            stream.readByteArray(fourCC) &&
            stream.read<quint8>(channelIndex));
}

void UploadIntervalBegin::printDebug(QDebug &dbg) const
{
    dbg << "SEND ClientUploadIntervalBegin{ GUID "
        << QString::fromUtf8(GUID.data(), GUID.size())
        << " fourCC"
        << QString::fromUtf8(fourCC.data(), fourCC.size())
        << "channelIndex: "
        << channelIndex
        << "}";
}

//+++++++++++++++++++++

UploadIntervalWrite::UploadIntervalWrite() :
    ClientMessage(MessageType::UploadIntervalWrite)
{

}

UploadIntervalWrite::UploadIntervalWrite(const MessageGuid &GUID, const QByteArray &encodedData, bool isLastPart) :
    ClientMessage(MessageType::UploadIntervalWrite),
    GUID(GUID),
    encodedData(encodedData),
    lastPart(isLastPart)
{

}

quint32 UploadIntervalWrite::getSerializePayload() const
{
    return sizeof(GUID) + sizeof(quint8) + NinjamOutputDataStream::getByteArrayPayload(encodedData);
}

bool UploadIntervalWrite::serializeTo(NinjamOutputDataStream& stream) const
{
    return (ClientMessage::serializeTo(stream) &&
            stream.writeByteArray(GUID) &&
            stream.write<quint8>(lastPart ? 1 : 0) && // If the Flag field bit 0 is set then the upload is complete.
            stream.writeByteArray(encodedData));
}

bool UploadIntervalWrite::unserializeFrom(NinjamInputDataStream& stream)
{
    quint8 lastPartFlag;
    if (stream.readByteArray(GUID) &&
            stream.read<quint8>(lastPartFlag)) {
        lastPart = lastPartFlag == 1;
        encodedData.resize(stream.getRemainingPayload());
        return stream.readByteArray(encodedData);
    }
    return false;
}

void UploadIntervalWrite::printDebug(QDebug &dbg) const
{
    dbg << "SEND ClientIntervalUploadWrite{"
        << "GUID="
        << QString::fromUtf8(GUID.data(), GUID.size())
        << ", encodedAudioBuffer= "
        << encodedData.size()
        << " bytes, isLastPart="
        << lastPart
        << '}';
}
