#include "src/config.h"

#include <QDir>
#include <QLoggingCategory>

namespace randomly {

Q_LOGGING_CATEGORY(lcConfig, "randomly.MyLauncher.Config")

Config::Config(QObject *parent)
    : QObject{parent}
    , m_settings("myLauncherCfg.ini", QSettings::Format::IniFormat)
{
    if (Q_UNLIKELY(m_settings.allKeys().isEmpty())) {
        qCInfo(lcConfig) << "resetting to default config";
        auto mcRoot = QDir(qEnvironmentVariable("HOME"));
        mcRoot.cd(".minecraft");

        m_settings.setValue("mcRoot", mcRoot.absolutePath());
        m_settings.setValue("launcher_name", "randomly.MyLauncher");
        m_settings.setValue("launcher_version", LAUNCHER_VERSION);
    }
}

QPointer<Config> Config::instance()
{
    static QPointer<Config> globalInstance = new Config;

    return globalInstance;
}

QVariant Config::getConfig(QString name)
{
    qCInfo(lcConfig) << "getting config" << name;
    return m_settings.value(name);
}

void Config::setConfig(QString name, QVariant value)
{
    qCInfo(lcConfig) << "setting config" << name << "->" << value.toString();
    m_settings.setValue(name, value);
}

} // namespace randomly
