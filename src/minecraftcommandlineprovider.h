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
    QStringList getArguments(QString versionName);
    QStringList readArguments(QFile &file);

    QString parseOption(const QString opt);

    std::optional<QStringList> handleConditionalArgument(QJsonObject arg);

    bool checkRules(QJsonArray rules);

    QStringList parseArgumentArray(QJsonArray arguments);
};

} // namespace randomly

#endif // MINECRAFTCOMMANDLINEPROVIDER_H
