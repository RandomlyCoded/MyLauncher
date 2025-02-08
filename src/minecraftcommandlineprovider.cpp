#include "src/minecraftcommandlineprovider.h"

#include "src/config.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonParseError>
#include <QLoggingCategory>
#include <optional>

namespace randomly {

Q_LOGGING_CATEGORY(lcCommandLineProvider, "randomly.MyLauncher.CmdLineProvider");

MinecraftCommandLineProvider::MinecraftCommandLineProvider(QObject *parent)
    : QObject{parent}
{}

QStringList MinecraftCommandLineProvider::getArguments(QString versionName)
{
    auto cfg = Config::instance();
    auto mcRoot = QDir{cfg->getConfig("mcRoot").toString()};

    mcRoot.cd("versions");
    mcRoot.cd(versionName);

    QFile mcConfig{mcRoot.filePath(versionName + ".json")};
    if (!mcConfig.open(QFile::ReadOnly)) {
        qCWarning(lcCommandLineProvider, "cannot open %ls: %ls", qUtf16Printable(mcConfig.fileName()), qUtf16Printable(mcConfig.errorString()));
        return {};
    }

    return readArguments(mcConfig);
}

QStringList MinecraftCommandLineProvider::readArguments(QFile &file)
{
    QJsonParseError jsonStatus;
    const auto versionConfig = QJsonDocument::fromJson(file.readAll(), &jsonStatus);

#warning we also need to handle libraries

    const auto jsonArguments = versionConfig["arguments"].toObject();

    const auto jsonGameArguments = jsonArguments["game"].toArray();
    const auto jsonJvmArguments = jsonArguments["jvm"].toArray();

    QStringList arguments{};

    arguments.append(parseArgumentArray(jsonGameArguments));

    if (const auto inherited = versionConfig["inheritsFrom"]; inherited != QJsonValue::Undefined) {
        qCInfo(lcCommandLineProvider) << file.fileName() << "inherits from" << inherited.toString();

        arguments += getArguments(inherited.toString());
    }

    return arguments;
}

QString MinecraftCommandLineProvider::parseOption(const QString opt)
{
    if (opt.startsWith('$')) {
        auto cfg = Config::instance();
        const auto l = opt.length();
        return cfg->getConfig(opt.left(l - 1).right(l - 3)).toString();
    }

    return opt;
}

std::optional<QStringList> MinecraftCommandLineProvider::handleConditionalArgument(QJsonObject arg)
{
    qCInfo(lcCommandLineProvider) << "conditional arg:" << arg;
    if (checkRules (arg["rules"].toArray())) {
        auto value = arg["value"];

        if (value.isString())
            return QStringList{parseOption(value.toString())};

        else {
            QStringList values;

            for (const auto &v: value.toArray())
                values.append(parseOption(v.toString()));
        }
    }

    return {};
}

bool MinecraftCommandLineProvider::checkRules(QJsonArray rules)
{
#warning implement rule handling
    return false;
}

QStringList MinecraftCommandLineProvider::parseArgumentArray(QJsonArray jsonArguments)
{
    QStringList arguments;

    for (const auto &arg: jsonArguments) {
        if (arg.isString())
            arguments.append(parseOption(arg.toString()));

        else {
            auto condArg = handleConditionalArgument(arg.toObject());
            if (condArg)
                arguments.append(condArg.value());
        }
    }

    return arguments;
}

std::optional<QPair<QString, QStringList>> MinecraftCommandLineProvider::getCommandLine(QString versionName)
{
    auto cfg = Config::instance();
    auto mcRoot = QDir{cfg->getConfig("mcRoot").toString()};

    mcRoot.cd("versions");
    mcRoot.cd(versionName);

    QFile mcConfig{mcRoot.filePath(versionName + ".json")};
    QFile launcherConfig{mcRoot.filePath("MyLauncher.json")};

    if (!launcherConfig.exists()) {} // skip launcher options or set some default values (maybe get them from Config? idk
                                     // i.e. screen, wrappers (like primusrun), different java executable...

    if (!mcConfig.open(QFile::ReadOnly)) {
        qCWarning(lcCommandLineProvider, "cannot open %ls: %ls", qUtf16Printable(mcConfig.fileName()), qUtf16Printable(mcConfig.errorString()));
        return {};
    }

    return QPair{"java", readArguments(mcConfig)};
}

} // namespace randomly
