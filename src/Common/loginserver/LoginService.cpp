#include "LoginService.h"
#include "natmap.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrlQuery>
#include <QNetworkAccessManager>
#include <QTimer>
#include <QDebug>
#include <QApplication>
#include "ninjam/client/ServerInfo.h"
#include "ninjam/client/Service.h"
#include "log/Logging.h"
#include "Configurator.h" // just to use VERSION
#include "Version.h"

using login::LoginService;
using login::UserInfo;
using login::RoomInfo;

const QString LoginService::LOGIN_SERVER_URL = "http://ninbot.com/app/servers.php";
const QString LoginService::VERSION_SERVER_URL = "http://jamtaba2.appspot.com/version";

namespace {

QNetworkRequest makeTimedRequest(const QString &url, int timeoutMs)
{
    QNetworkRequest request{QUrl(url)};
    request.setTransferTimeout(timeoutMs);
    return request;
}

}

UserInfo::UserInfo(const QString &name, const QString &ip, const QString &countryName, const QString &countryCode, float latitude, float longitude) :
    name(name),
    ip(ip)
{
    location.latitude = latitude;
    location.longitude = longitude;
    location.countryCode = countryCode;
    location.countryName = countryName;
}

RoomInfo::RoomInfo(const QString &roomName, int roomPort,
                   int maxUsers, const QList<UserInfo> &users, int maxChannels, int bpi, int bpm, const QString &streamUrl) :
    name(roomName),
    port(roomPort),
    maxUsers(maxUsers),
    maxChannels(maxChannels),
    users(users),
    bpi(bpi),
    bpm(bpm),
    streamUrl(streamUrl),
    isPrivate(false)
{
    //
}

RoomInfo::RoomInfo(const QString &roomName, int roomPort, int maxUsers, int maxChannels) :
    RoomInfo(roomName, roomPort, maxUsers, QList<login::UserInfo>(), maxChannels, 0, 0, "")
{
    isPrivate = true;
}

void RoomInfo::setPreferredUserCredentials(const QString userName, const QString userPass)
{
    userCredentials.name = userName;
    userCredentials.pass = userPass;
}

bool RoomInfo::hasPreferredUserCredentials() const
{
    return !userCredentials.name.isEmpty();
}

QString RoomInfo::getUniqueName() const {
    return QString("%1:%2").arg(getName(), QString::number(getPort()));
}

int RoomInfo::getNonBotUsersCount() const
{
    int nonBots = 0;
    for (const UserInfo &userInfo : users) {
        if (!ninjam::client::Service::isBotName(userInfo.getName()))
            nonBots++;
    }
    return nonBots;
}

bool RoomInfo::isEmpty() const
{
    return getNonBotUsersCount() == 0;
}

// +++++++++++++++++++++++++++++++++++++++++++++++++++++

LoginService::LoginService(QObject *parent) :
    QObject(parent),
    httpClient(this),
    refreshTimer(new QTimer(this))
{
    connect(refreshTimer, &QTimer::timeout, this, [=](){
        httpClient.get(makeTimedRequest(LOGIN_SERVER_URL, REQUEST_TIMEOUT_MS));
    });

    connect(&httpClient, &QNetworkAccessManager::finished, this, [=](QNetworkReply *reply){
        const auto requestUrl = reply->request().url();
        if (reply->error() != QNetworkReply::NoError) {
            qWarning() << "LoginService request failed:" << requestUrl << reply->errorString();
            if (requestUrl == QUrl(LOGIN_SERVER_URL))
                emit roomsListAvailable(QList<RoomInfo>());
            reply->deleteLater();
            return;
        }

        handleJson(reply->readAll(), requestUrl);
        reply->deleteLater();
    });

    refreshTimer->start(REFRESH_PERIOD);

    // the first public servers list query
    httpClient.get(makeTimedRequest(LOGIN_SERVER_URL, REQUEST_TIMEOUT_MS));

    // query the current version to jamtaba server using HTTP (is querying github using HTTPS)
    httpClient.get(makeTimedRequest(VERSION_SERVER_URL, REQUEST_TIMEOUT_MS));

}

void LoginService::handleServersJson(const QJsonObject &root)
{
    QJsonArray allRooms = root["servers"].toArray();
    QList<RoomInfo> publicRooms;
    for (int i = 0; i < allRooms.size(); ++i) {
        QJsonObject jsonObject = allRooms[i].toObject();
        auto roomInfo = buildRoomInfoFromJson(jsonObject);
        publicRooms.append(roomInfo);
    }
    emit roomsListAvailable(publicRooms);
}
void LoginService::handleVersionJson(const QJsonObject &root)
{
    auto versionTag = root.value("version").toString("error");
    auto versionDetails = root.value("details").toString("error");
    auto publicationDate = root.value("published_at").toString("");

    auto currentVersion = loginserver::Version::fromString(APP_VERSION);
    auto latestVersion = loginserver::Version::fromString(versionTag);

    if (latestVersion.isNewerThan(currentVersion))
        emit newVersionAvailableForDownload(versionTag, publicationDate, versionDetails);

}

void LoginService::handleJson(const QString &json, const QUrl &requestUrl)
{
    const bool isServersRequest = requestUrl == QUrl(LOGIN_SERVER_URL);

    if (json.isEmpty()) {
        if (isServersRequest)
            emit roomsListAvailable(QList<RoomInfo>());
        return;
    }

    auto document = QJsonDocument::fromJson(QByteArray(json.toStdString().c_str()));
    auto root = document.object();

    if (root.contains("servers"))
        handleServersJson(root);
    else if (root.contains("version"))
        handleVersionJson(root);
    else if (isServersRequest)
        emit roomsListAvailable(QList<RoomInfo>());
}

int getServerPort(const QString &serverName) {
    auto port = 2049;
    auto index = serverName.lastIndexOf(":");

    if (index) {
        return serverName.rightRef(serverName.size() - (index + 1)).toInt();
    }

    return port;
}

QString getServerName(const QString &serverText) {
    auto index = serverText.lastIndexOf(":");

    if (index) {
        return serverText.left(index);
    }

    return serverText;
}

int getServerGuessedMaxUsers(const QString &serverName, int serverPort) {
    if (serverName.contains("discordonlinejammingcentral"))
        return 10;

    if (serverName.contains("ninbot") && serverPort == 2051)
        return 16;

    if (serverName.contains("musicorner") || serverName.contains("cockos"))
        return 4;

    return 8;
}

RoomInfo LoginService::buildRoomInfoFromJson(const QJsonObject &jsonObject)
{
    auto serverNameText = jsonObject.value("name").toString("Error");
    auto name =  getServerName(serverNameText);
    int port = getServerPort(serverNameText);
    int maxUsers = jsonObject.contains("user_max") ? jsonObject["user_max"].toString().toInt() : getServerGuessedMaxUsers(name, port);
    if (name.contains(QRegExp("discordonlinejammingcentral|mutant"))) maxUsers--; // ugly hack to get bots subtracted from maxusers in jamroomview
    auto streamLink = jsonObject.value("stream").toString("");
    int bpi = jsonObject.contains("bpi") ? jsonObject["bpi"].toString().toInt() : 16;
    int bpm = jsonObject.contains("bpm") ? jsonObject["bpm"].toString().toInt() : 120;

    auto usersArray = jsonObject.value("users").toArray();

    QList<UserInfo> users;
    users.reserve(usersArray.size());
    for (int i = 0; i < usersArray.size(); ++i) {
        auto userObject = usersArray[i].toObject();
        auto userName = userObject.value("name").toString("Error");
        auto userIp = userObject.value("ip").toString("Error");
        float latitude = userObject.value("lat").toString("0").toFloat();
        float longitude = userObject.value("lon").toString("0").toFloat();
        auto countryName = userObject.value("country").toString("");
        auto countryCode = userObject.value("co").toString("");
        users.append(login::UserInfo(userName, userIp, countryName, countryCode, latitude, longitude));
    }
    return RoomInfo(name, port, maxUsers, users, 0, bpi, bpm, streamLink);
}
