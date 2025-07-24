#ifndef MINECRAFTCOMMANDLINEPROVIDER_H
#define MINECRAFTCOMMANDLINEPROVIDER_H

#include <QFile>
#include <QObject>

namespace randomly {

class DownloadInfo;
class Downloader;

class MinecraftCommandLineProvider : public QObject
{
    Q_OBJECT

public:
    explicit MinecraftCommandLineProvider(QObject *parent = nullptr);

    std::optional<QPair<QString, QStringList>> getCommandLine(QString versionName);

private:
    QStringList readArguments(const QString versionName);
    QJsonDocument loadJsonFromVersion(const QString versionName);
    QJsonDocument getCombinedVersionConfig(const QString rootVersion);
    void tryRecursivelyMergingObjects(QJsonObject &lhs, const QJsonObject &rhs);

    QStringList parseArgumentArray(QJsonArray arguments);
    QString parseOption(const QString opt);

    std::optional<QStringList> handleConditionalArgument(QJsonObject arg);
    bool checkRules(const QJsonArray rules);
    bool confirmOs(const QJsonObject &rule);
    bool confirmFeatures(const QJsonObject &rule);
    bool safelyCheckValue(const QJsonObject &value, QString key, QRegularExpression expected);

    QString collectClassPath(const QJsonDocument &versionConfig);
    QString generateRelativePathFromName(const QString libraryName);
    std::optional<QString> getArtifactPath(const QJsonObject library);

    void downloadLibraries(const QJsonDocument &versionConfig);
    void prepareNativesDownload(QJsonObject classifiers);

    void loadAssets(const QJsonObject &assetIndex);
    void maybeFinalizeAssetDownload(const DownloadInfo &info);
    bool verifyFileSha1(const QString filename, const QString expectedSha1);

    Downloader *m_downloads;
};

} // namespace randomly

#endif // MINECRAFTCOMMANDLINEPROVIDER_H
