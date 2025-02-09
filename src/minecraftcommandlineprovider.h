#ifndef MINECRAFTCOMMANDLINEPROVIDER_H
#define MINECRAFTCOMMANDLINEPROVIDER_H

#include <QFile>
#include <QObject>

namespace randomly {

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
    bool checkRules(QJsonArray rules);

    void collectClassPath(QString &cp, const QJsonDocument &versionConfig);
};

} // namespace randomly

#endif // MINECRAFTCOMMANDLINEPROVIDER_H
