#include "minecraftcommandlineprovider.h"

#include "config.h"
#include "downloader.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLoggingCategory>
#include <optional>

namespace randomly {

Q_LOGGING_CATEGORY(lcCommandLineProvider, "randomly.MyLauncher.CmdLineProvider");

MinecraftCommandLineProvider::MinecraftCommandLineProvider(QObject *parent)
    : QObject{parent}
    , m_downloads{new Downloader(this)}
{}

std::optional<QPair<QString, QStringList>> MinecraftCommandLineProvider::getCommandLine(QString versionName)
{
    auto cfg = Config::instance();
    auto mcRoot = QDir{cfg->getConfig("mcRoot").toString()};

    mcRoot.cd("versions");
    mcRoot.cd(versionName);

    QFile launcherConfig{mcRoot.filePath("MyLauncher.json")};

    // also get the instance info from .minecraft/launcher_profiles.json. pass using an argument of type QJsonDOcument/Object?

    if (!launcherConfig.exists()) {} // skip launcher options or set some default values (maybe get them from Config? idk
    // i.e. screen, wrappers (like primusrun), different java executable...

    return QPair<QString, QStringList>{"/usr/bin/java", readArguments(versionName)};
}

QStringList MinecraftCommandLineProvider::readArguments(const QString versionName)
{
    auto cfg = Config::instance();
    auto mcRoot = QDir{cfg->getConfig("mcRoot").toString()};

    QStringList arguments{};

    auto mergedConfig = getCombinedVersionConfig(versionName);

    // mergedConfig[downloads] yields client/server jar (save in versions/version/version.jar and pass on launch)

    cfg->setTemp("assets_index_name", mergedConfig["assetIndex"].toObject()["id"].toString());
    cfg->setTemp("assets_root", cfg->getConfig("mcRoot").toString() + "/assets");

    loadAssets(mergedConfig["assetIndex"].toObject());

    // store information we might need later as temporary configs
    cfg->setTemp("version_name", mergedConfig["id"]);
    cfg->setTemp("game_directory", mcRoot.absolutePath());
    cfg->setTemp("assets_dir", mcRoot.absoluteFilePath("assets"));
    cfg->setTemp("assets_index_name", mergedConfig["assetIndex"]["id"]);
    cfg->setTemp("version_type", mergedConfig["type"]);

    cfg->setTemp("natives_directory", mcRoot.absoluteFilePath("bin/" + cfg->getTemp("version_name").toString()));
    if (const auto nativesDir = QDir(cfg->getTemp("natives_directory").toString()); !nativesDir.exists())
        nativesDir.mkpath(".");

    cfg->setTemp("classpath", collectClassPath(mergedConfig));

    // schedule downloads while parsing the arguments
    downloadLibraries(mergedConfig);

    // the argument order is: jvm, logging, mainClass, game
    arguments += parseArgumentArray(mergedConfig["arguments"]["jvm"].toArray());
    arguments += mergedConfig["mainClass"].toString();
    arguments += parseArgumentArray(mergedConfig["arguments"]["game"].toArray());

    return arguments;
}

QJsonDocument MinecraftCommandLineProvider::loadJsonFromVersion(const QString versionName)
{
    auto cfg = Config::instance();
    auto mcRoot = QDir{cfg->getConfig("mcRoot").toString()};

    mcRoot.cd("versions");
    mcRoot.cd(versionName);

    QFile configFile{mcRoot.filePath(versionName + ".json")};

    if (!configFile.open(QFile::ReadOnly)) {
        qCWarning(lcCommandLineProvider, "cannot open config for version %ls (%ls): %ls", qUtf16Printable(versionName), qUtf16Printable(configFile.fileName()), qUtf16Printable(configFile.errorString()));
        return {};
    }

    return QJsonDocument::fromJson(configFile.readAll());
}

QJsonDocument MinecraftCommandLineProvider::getCombinedVersionConfig(const QString rootVersion)
{
    auto config = loadJsonFromVersion(rootVersion);

    auto newDoc = QJsonDocument{};

    for(auto inherited = config["inheritsFrom"]; inherited != QJsonValue::Undefined; inherited = newDoc["inheritsFrom"]) {
        newDoc = loadJsonFromVersion(inherited.toString());
        qCInfo(lcCommandLineProvider) << "inheriting" << inherited.toString();

        auto configObj = config.object();
        tryRecursivelyMergingObjects(configObj, newDoc.object());

        config.setObject(configObj);
    }

    return config;
}

void MinecraftCommandLineProvider::tryRecursivelyMergingObjects(QJsonObject &lhs, const QJsonObject &rhs)
{
    const auto keys = rhs.keys();

    for (const auto &key: keys) {
        // the simplest case: the key is not in lhs, so just insert it
        if (!lhs.contains(key))
            lhs[key] = rhs[key];

        else {
            auto lValue = lhs[key];

            // arrays and objects need special handling, so we check the type of the value
            switch (lValue.type()) {
            case QJsonValue::Bool:
            case QJsonValue::Double:
            case QJsonValue::String:

                // Null and Undefined technically shouldn't appear, but clang wants to handle them
            case QJsonValue::Null:
            case QJsonValue::Undefined:

                // primitives don't get changed, so just skip them
                break;

            case QJsonValue::Array: {
                // arrays are just combined, nothing fancy here
                auto lArray = lValue.toArray();
                const auto rValue = rhs[key];

                if (rValue.type() != QJsonValue::Array)
                    lArray.append(rValue);

                else {
                    const auto rArray = rValue.toArray();
                    for (const auto rv: rArray)
                        lArray.append(rv);
                }

                lhs[key] = lArray;
            } break;

            case QJsonValue::Object: {
                // objects are merged, that's why this function is called "recursively"
                auto lObj = lValue.toObject();
                const auto rObj = rhs[key].toObject();
                tryRecursivelyMergingObjects(lObj, rObj);

                lhs[key] = lObj;
            }

            } // switch ends here
        }
    }
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

QString MinecraftCommandLineProvider::parseOption(const QString opt)
{
    if (!opt.contains('$'))
        return opt;

    QString parsed;
    auto cfg = Config::instance();

    const auto slices = opt.split('$');

    for (const auto &slice: slices) {
        if (slice.isEmpty())
            continue;

        if (slice.startsWith('{')) {
            // trim off left end right curly brackets
            const auto l = slice.length();
            const auto name = slice.left(l - 1).right(l - 2);

            parsed += cfg->getAny(name).toString();
        } else
            parsed += slice;
    }

    qCInfo(lcCommandLineProvider) << opt << "->" << parsed;

    return parsed;
}

std::optional<QStringList> MinecraftCommandLineProvider::handleConditionalArgument(QJsonObject arg)
{
    qCInfo(lcCommandLineProvider) << "conditional arg:" << arg;

    if (!checkRules (arg["rules"].toArray()))
        return {};

    auto value = arg["value"];

    if (value.isString())
        return QStringList{parseOption(value.toString())};

    QStringList values;

    const auto jsonValues = value.toArray();
    for (const auto &v: jsonValues)
        values.append(parseOption(v.toString()));

    return values;
}

bool MinecraftCommandLineProvider::checkRules(const QJsonArray rules)
{
    const auto cfg = Config::instance();

    bool applies = true;

    // I only know of rules checking the OS, so that's the only thing I'm implementing
    for (int i = 0; i < rules.size(); ++i) {
        const auto rule = rules.at(i).toObject();

        auto confirmed = confirmOs(rule) && confirmFeatures(rule);

        if (rule["action"] == "disallow")
            confirmed ^= 1;

        applies &= confirmed;
    }

    return applies;
}

bool MinecraftCommandLineProvider::confirmOs(const QJsonObject &rule)
{
    if (!rule.contains("os"))
        return true;

    const auto cfg = Config::instance();
    const auto os = rule["os"].toObject();

    return safelyCheckValue(os, "name", QRegularExpression{cfg->getConfig("os_name").toString()}) &&
           safelyCheckValue(os, "version", QRegularExpression{cfg->getConfig("os_version").toString()}) &&
           safelyCheckValue(os, "arch", QRegularExpression{cfg->getConfig("os_arch").toString()});
}

bool MinecraftCommandLineProvider::confirmFeatures(const QJsonObject &rule)
{
    // probably just a lookup in config?
    return !rule.contains("features");
}

bool MinecraftCommandLineProvider::safelyCheckValue(const QJsonObject &value, QString key, QRegularExpression expected)
{
    if (!value.contains(key))
        return true;

    return expected.match(value["key"].toString()).hasMatch();
}

QString MinecraftCommandLineProvider::collectClassPath(const QJsonDocument &versionConfig)
{
    QString cp;

    QSet<QString> librarySet;

    const auto cfg = Config::instance();
    auto libraryRoot = QDir{cfg->getConfig("mcRoot").toString()};
    libraryRoot.cd("libraries");

    const auto libraryInfoArray = versionConfig["libraries"].toArray();

    for (int i = 0; i < libraryInfoArray.size(); ++i) {
        const auto library = libraryInfoArray.at(i).toObject();

        // avoid duplicates
        const auto libraryName = library["name"].toString();
        if (librarySet.contains(libraryName))
            continue;

        librarySet.insert(libraryName);

        auto path = getArtifactPath(library);
        if (!path.has_value())
            continue;

        cp += libraryRoot.absoluteFilePath(path.value());
        cp += ":";
    }

    auto mcVersionDir = QDir{cfg->getConfig("mcRoot").toString()};
    mcVersionDir.cd("versions");
    const auto versionName = cfg->getTemp("version_name").toString();
    mcVersionDir.cd(versionName);

    return cp + mcVersionDir.absoluteFilePath(versionName + ".jar");
}

QString MinecraftCommandLineProvider::generateRelativePathFromName(const QString libraryName)
{
    auto slices = libraryName.split(":");
    slices[0].replace('.', '/');

    return QString("%1/%2/%3/%2-%3.jar").arg(slices[0], slices[1], slices[2]);
}

std::optional<QString> MinecraftCommandLineProvider::getArtifactPath(const QJsonObject library)
{
    // if there are rules, check them. If they don't allow this library, skip it.
    if (const auto rules = library["rules"]; rules != QJsonValue::Undefined &&
                                             !checkRules(rules.toArray()))
        return {};

    const auto download = library["downloads"];
    const auto libraryName = library["name"].toString();

    if (download != QJsonValue::Undefined) // simple download
        return download["artifact"]["path"].toString();

    else
        return generateRelativePathFromName(libraryName);
}

void MinecraftCommandLineProvider::downloadLibraries(const QJsonDocument &versionConfig)
{
    qCInfo(lcCommandLineProvider) << "downloading libraries...";

    // previously used to avoid duplicates
    // QSet<QString> librarySet;

    const auto cfg = Config::instance();
    auto libraryRoot = QDir{cfg->getConfig("mcRoot").toString() + "/libraries"};

    const auto libraryInfoArray = versionConfig["libraries"].toArray();

    for (int i = 0; i < libraryInfoArray.size(); ++i) {
        const auto library = libraryInfoArray.at(i).toObject();

        // avoid duplicates
        // doesn't work, since some libraries have more than one entry, which sometimes contains crucial information
        /*
        const auto libraryName = library["name"].toString();
        if (librarySet.contains(libraryName))
            continue;

        librarySet.insert(libraryName); */

        const auto path = getArtifactPath(library);
        if (!path.has_value())
            continue;
        const auto absolutePath = libraryRoot.absoluteFilePath(path.value());

        const auto download = library["downloads"];
        QJsonValue artifact;
        if (download != QJsonValue::Undefined) // simple download
            artifact = download["artifact"];
        else
            artifact = library;

        if (!QFile(absolutePath).exists()) { // QDir gives false negatives
            DownloadInfo info;
            info.path = absolutePath;
            info.url = artifact["url"].toString();
            if (info.url.endsWith('/'))
                info.url += getArtifactPath(library).value();

            info.sha1 = artifact["sha1"].toString();
            info.size = artifact["size"].toInteger();

            m_downloads->download(info);
        }

        if (download != QJsonValue::Undefined)
            if (auto classifiers = download["classifiers"]; classifiers != QJsonValue::Undefined)
                if (classifiers.toObject().contains("natives-" + cfg->getConfig("os_name").toString()))
                    prepareNativesDownload(classifiers.toObject());
    }
}

void MinecraftCommandLineProvider::prepareNativesDownload(QJsonObject classifiers)
{
    const auto natives = classifiers["natives-" + Config::instance()->getConfig("os_name").toString()].toObject();

    const auto cfg = Config::instance();
    auto libraryRoot = QDir{cfg->getConfig("mcRoot").toString() + "/libraries/"};

    DownloadInfo download;

    download.path = libraryRoot.absoluteFilePath(natives["path"].toString());
    if (QFile::exists(download.path)) // archive already exists, so we'll assume it is also extracted
        return;

    download.sha1 = natives["sha1"].toString();
    download.size = natives["size"].toInt();
    download.url  = natives["url"].toString();
    download.native = true;

    qCInfo(lcCommandLineProvider).noquote() << "downloading natives" << download.path;
    m_downloads->download(download);
}

void MinecraftCommandLineProvider::loadAssets(const QJsonObject &assetIndex)
{
    connect(m_downloads, &Downloader::downloadCompleted, this, &MinecraftCommandLineProvider::maybeFinalizeAssetDownload);

    DownloadInfo download;
    download.path = Config::instance()->getTemp("assets_root").toString() + "/indexes/" + assetIndex["id"].toString() + ".json";
    download.sha1 = assetIndex["sha1"].toString();
    download.size = assetIndex["size"].toInt();
    download.url  = assetIndex["url"].toString();
    download.assets = true;

    if (QFile::exists(download.path)) {
        // verify assets are clean
        qInfo() << "verifying assets";
        maybeFinalizeAssetDownload(download);
        return;
    }

    qCInfo(lcCommandLineProvider).noquote() << "downloading asset index" << download.path;
    m_downloads->download(download);
}

void MinecraftCommandLineProvider::maybeFinalizeAssetDownload(const DownloadInfo &info)
{
    if (!info.assets)
        return;

    auto cfg = Config::instance();

    qInfo() << "received reply for natives!" << info.url << info.path;

    QFile index(info.path);
    if (!index.open(QFile::ReadOnly)) {
        qCWarning(lcCommandLineProvider) << "failed to open" << info.path << ":" << index.errorString();
        return;
    }

    QJsonObject objects = QJsonDocument::fromJson(index.readAll())["objects"].toObject();

    for (const auto &key: objects.keys()) {
        const auto obj = objects[key].toObject();
        DownloadInfo download;
        download.sha1 = obj["hash"].toString();
        download.size = obj["size"].toInt();

        const auto assetPath = download.sha1.sliced(0, 2) + "/" + download.sha1;

        download.url = "https://resources.download.minecraft.net/" + assetPath;
        download.path = cfg->getConfig("mcRoot").toString() + "/assets/objects/" + assetPath;
        if (QFile::exists(download.path))
            if (verifyFileSha1(download.path, download.sha1))
                continue;

        m_downloads->download(download);
    }
}

bool MinecraftCommandLineProvider::verifyFileSha1(const QString filename, const QString expectedSha1)
{
    QCryptographicHash sha1(QCryptographicHash::Sha1);
    QFile f(filename);
    if (!f.open(QFile::ReadOnly))
        return false;

    sha1.addData(f.readAll());

    const auto hashResult = sha1.result().toHex();

    if (hashResult != expectedSha1.toLocal8Bit())
        return false;

    return true;
}

} // namespace randomly
