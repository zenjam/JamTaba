#ifndef UPNPMANAGER_H
#define UPNPMANAGER_H

#include <QObject>

#include "miniupnpc.h"

class UPnPManager : public QObject
{
    Q_OBJECT

public:
    UPnPManager();
    ~UPnPManager();
    bool openPort(quint16 port);
    bool closePort(quint16 port);
    QString getExternalIp() const;

signals:
    void portOpened(const QString &localIP, const QString &externalIP);
    void portClosed();
    void errorDetected(const QString &msg);

private:
    UPNPDev *devlist;
    UPNPUrls urls;
    IGDdatas data;

    QString externalIP;
};

#endif // UPNPMANAGER_H
