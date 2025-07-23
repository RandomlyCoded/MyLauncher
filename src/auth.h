#ifndef AUTH_H
#define AUTH_H

#include <QEventLoop>
#include <QObject>


class QNetworkAccessManager;

class QNetworkReply;
namespace randomly {

class Auth : public QObject
{
    Q_OBJECT

public:
    explicit Auth(QObject *parent = nullptr);

    void updateAccessToken();

    void obtainMinecraftToken();

private:
    bool upToDate() { return false; } // FIXME: actual upToDate auth logic lol
    QString getUserHash(const QJsonObject &jsonObj);

    QJsonDocument prepareXBoxAuthPayload();
    QJsonDocument prepareXstsAuthPayload(const QString &xblToken);

    void receiveXBoxReply(QNetworkReply *reply);
    void receiveXstsReply(QNetworkReply *reply);
    void receiveMinecraftReply(QNetworkReply *reply);
    void receiveProfileReply(QNetworkReply *reply);

    QEventLoop m_authLoop;

    QNetworkAccessManager *m_ctrl;
};

} // namespace randomly

#endif // AUTH_H
