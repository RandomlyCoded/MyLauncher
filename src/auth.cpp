#include "auth.h"
#include "config.h"

#include <QDesktopServices>
#include <QOAuth2AuthorizationCodeFlow>
#include <QOAuthHttpServerReplyHandler>
#include <QGuiApplication>
#include <QLoggingCategory>
#include <QNetworkRequest>
#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonObject>
#include <QJsonArray>
#include <QRandomGenerator>

namespace randomly {

namespace
{

Q_LOGGING_CATEGORY(lcAuth, "randomly.MyLauncher.Auth");

} // namespace

Auth::Auth(QObject *parent)
    : QObject{parent}
    , m_ctrl(new QNetworkAccessManager(this))
{}

void Auth::obtainMinecraftToken()
{
    qCInfo(lcAuth) << "getting Minecraft token";

    /*
     * Steps:
     * - MSFT auth (access token)
     * - XBox auth (xbl/XBoxLive token)
     * - Xsts auth
     * - Minecraft auth
     * - get Minecraft profile
     */

    if (!upToDate())
        updateAccessToken();

    QNetworkRequest xBoxAuthReq;
    xBoxAuthReq.setHeaders(QHttpHeaders::fromListOfPairs(
        {
            {"Content-Type", "application/json"},
            {"Accept", "application/json"}
        }
    ));
    xBoxAuthReq.setUrl(QUrl{"https://user.auth.xboxlive.com/user/authenticate"});

    QJsonDocument xBoxAuthJson = prepareXBoxAuthPayload();
    connect(m_ctrl, &QNetworkAccessManager::finished, this, &Auth::receiveXBoxReply);

    m_ctrl->post(xBoxAuthReq, xBoxAuthJson.toJson(QJsonDocument::Compact));

    qCInfo(lcAuth) << "waiting for XBox/Minecraft auth";
    m_authLoop.exec(); // wait for auth to finish
}

QJsonDocument Auth::prepareXBoxAuthPayload()
{
    QJsonObject payload;

    QJsonObject properties;
    properties["AuthMethod"] = "RPS";
    properties["SiteName"] = "user.auth.xboxlive.com";
    properties["RpsTicket"] = "d=" + Config::instance()->getTemp("accessToken").toString();

    payload["Properties"] = properties;
    payload["RelyingParty"] = "http://auth.xboxlive.com";
    payload["TokenType"] = "JWT";

    return QJsonDocument(payload);
}

QJsonDocument Auth::prepareXstsAuthPayload(const QString &xblToken)
{
    QJsonObject payload;

    QJsonObject properties;
    properties["SandboxId"] = "RETAIL";
    properties["UserTokens"] = QJsonArray::fromStringList({xblToken});

    payload["Properties"] = properties;
    payload["RelyingParty"] = "rp://api.minecraftservices.com/";
    payload["TokenType"] = "JWT";

    return QJsonDocument(payload);
}

QString Auth::getUserHash(const QJsonObject &jsonObj)
{
    return jsonObj["DisplayClaims"].toObject()["xui"].toArray()[0].toObject()["uhs"].toString();
}

void Auth::receiveXBoxReply(QNetworkReply *reply)
{
    qCInfo(lcAuth) << "received XBox auth!";
    auto xBoxReply = QJsonDocument::fromJson(reply->readAll()).object();

    const auto xblToken = xBoxReply["Token"];

    Config::instance()->setTemp("userhash", getUserHash(xBoxReply));

    // XSTS auth
    QNetworkRequest xstsAuthReq;
    xstsAuthReq.setHeaders(QHttpHeaders::fromListOfPairs(
        {
            {"Content-Type", "application/json"},
            {"Accept", "application/json"}
        }
    ));
    xstsAuthReq.setUrl(QUrl{"https://xsts.auth.xboxlive.com/xsts/authorize"});

    QJsonDocument xstsAuthJson = prepareXstsAuthPayload(xblToken.toString());

    disconnect(m_ctrl, &QNetworkAccessManager::finished, this, &Auth::receiveXBoxReply);

    connect(m_ctrl, &QNetworkAccessManager::finished, this, &Auth::receiveXstsReply);
    m_ctrl->post(xstsAuthReq, xstsAuthJson.toJson(QJsonDocument::Compact));

    qCInfo(lcAuth) << "waiting for xsts auth";
}

void Auth::receiveXstsReply(QNetworkReply *reply)
{
    qCInfo(lcAuth) << "received XSTS auth!";
    auto xstsReply = QJsonDocument::fromJson(reply->readAll()).object();

    if (getUserHash(xstsReply) != Config::instance()->getTemp("userhash")) {
        qCFatal(lcAuth).noquote().nospace() << "userhash does NOT match! (" << getUserHash(xstsReply) << " / " << Config::instance()->getTemp("userhash").toString();
    }

    const auto xstsToken = xstsReply["Token"].toString();

    // Minecraft auth
    QNetworkRequest mcAuthReq;
    mcAuthReq.setHeaders(QHttpHeaders::fromListOfPairs(
        {
            {"Content-Type", "application/json"},
            {"Accept", "application/json"}
        }
        ));
    mcAuthReq.setUrl(QUrl{"https://api.minecraftservices.com/authentication/login_with_xbox"});

    QJsonObject mcAuthInfo;
    mcAuthInfo["identityToken"] = QString("XBL3.0 x=%1;%2").arg(getUserHash(xstsReply), xstsToken);

    auto mcAuthJson = QJsonDocument(mcAuthInfo);

    disconnect(m_ctrl, &QNetworkAccessManager::finished, this, &Auth::receiveXstsReply);

    connect(m_ctrl, &QNetworkAccessManager::finished, this, &Auth::receiveMinecraftReply);
    m_ctrl->post(mcAuthReq, mcAuthJson.toJson(QJsonDocument::Compact));

    qCInfo(lcAuth) << "waiting for minecraft auth";
}

void Auth::receiveMinecraftReply(QNetworkReply *reply)
{
    qCInfo(lcAuth) << "received Minecraft auth!";
    auto cfg = Config::instance();
    auto mcReply = QJsonDocument::fromJson(reply->readAll()).object();

    cfg->setTemp("auth_access_token", mcReply["access_token"].toString());
    cfg->setTemp("auth_xuid", cfg->getTemp("userhash"));

    // account info (this is what we actually want!)
    QNetworkRequest profileAuthReq;
    profileAuthReq.setHeaders(QHttpHeaders::fromListOfPairs(
        {
         {"Authorization", ("Bearer " + cfg->getTemp("auth_access_token").toString()).toUtf8()}
        }
    ));
    profileAuthReq.setUrl(QUrl{"https://api.minecraftservices.com/minecraft/profile"});

    disconnect(m_ctrl, &QNetworkAccessManager::finished, this, &Auth::receiveMinecraftReply);

    connect(m_ctrl, &QNetworkAccessManager::finished, this, &Auth::receiveProfileReply);
    m_ctrl->get(profileAuthReq);

    qCInfo(lcAuth) << "waiting for profile info";
}

void Auth::receiveProfileReply(QNetworkReply *reply)
{
    qCInfo(lcAuth) << "received profile reply!";

    auto profile = QJsonDocument::fromJson(reply->readAll()).object();
    auto cfg = Config::instance();

    cfg->setTemp("auth_player_name", profile["name"].toString());
    cfg->setTemp("auth_uuid", profile["id"].toString());

    // the clientId looks like random characters to me, and I didn't find any clientId in any response (except msa, but that's the prismLauncher clientId"
    QString clientid;
    auto rng = QRandomGenerator::global();
    for (int i = 0; i < 24; ++i) {
        auto rnd = rng->generate();
        clientid += char((rnd % 26 + 65) | (rnd & 32));
    }

    cfg->setTemp("clientid", clientid);

    qCInfo(lcAuth) << "authentification done!";

    m_authLoop.quit(); // finally "return" to Auth::obtainMinecraftToken
}

void Auth::updateAccessToken()
{
    if (upToDate())
        return;

    qCInfo(lcAuth) << "refreshing MSFT access token";

    auto cfg = Config::instance();
    const auto refreshToken = cfg->getConfig("refreshToken");

    QOAuth2AuthorizationCodeFlow oauth;
    QEventLoop authLoop;

    QObject::connect(&oauth, &QAbstractOAuth::authorizeWithBrowser, this, &QDesktopServices::openUrl);
    QObject::connect(&oauth, &QAbstractOAuth::granted, this, [&oauth, &authLoop]() {
        qCInfo(lcAuth) << "clientID:" << oauth.clientIdentifier() << Qt::endl
                       << "token:" << oauth.token() << Qt::endl
                       << "refreshToken:" << oauth.refreshToken() << Qt::endl
                       << "extraTokens:" << oauth.extraTokens();

        auto cfg = Config::instance();

        cfg->setConfig("refreshToken", oauth.refreshToken());
        cfg->setTemp("accessToken", oauth.token());
        cfg->setConfig("extraTokens", oauth.extraTokens());

        authLoop.quit();
    });

    auto replyHandler = new QOAuthHttpServerReplyHandler(this);
    // maybe redirect to randomlycoded.github.io/myLauncher/successful-login (gotta make that site tho)

    replyHandler->setCallbackText(QString(R"XXX(
    <noscript>
      <meta http-equiv="Refresh" content="0; URL=https://prismlauncher.org/successful-login" />
    </noscript>
    Login Successful, redirecting...
    <script>
      window.location.replace("https://prismlauncher.org/successful-login");
    </script>
    )XXX"));

    oauth.setAuthorizationUrl(QUrl{"https://login.microsoftonline.com/consumers/oauth2/v2.0/authorize"});
    oauth.setAccessTokenUrl(QUrl{"https://login.microsoftonline.com/consumers/oauth2/v2.0/token"});
    oauth.setClientIdentifier("c36a9fb6-4f2a-41ff-90bd-ae7cc92031eb"); // prism
    oauth.setScope("XboxLive.SignIn XboxLive.offline_access");
    oauth.setReplyHandler(replyHandler);

    if (refreshToken.isNull()) { // initial auth required
        qCInfo(lcAuth) << "getting initial auth";
        // Initiate the authorization
        oauth.grant();

    } else {
        qCInfo(lcAuth) << "refreshing auth";

        oauth.setRefreshToken(refreshToken.toString());
        oauth.refreshAccessToken();
    }

    authLoop.exec(); // wait for auth to finish
    qCInfo(lcAuth) << "auth finished";

    QGuiApplication::quit();
}

} // namespace randomly
