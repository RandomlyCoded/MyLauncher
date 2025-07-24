#include "minecraftcommandlineprovider.h"

#include "auth.h"
#include "config.h"

#include <QDir>
#include <QGuiApplication>
#include <QLoggingCategory>
#include <QProcess>
#include <QQmlApplicationEngine>

namespace randomly {

class Application : public QGuiApplication
{
public:
    using QGuiApplication::QGuiApplication;

    int run();
};

class Backend : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QStringList versions READ versions CONSTANT FINAL)
    Q_PROPERTY(bool mcRunning READ running NOTIFY runningChanged FINAL)

public:
    Backend() {
        QDir d(Config::instance()->getConfig("mcRoot").toString() + "/versions");
        m_versions = d.entryList(QDir::Filter::Dirs | QDir::Filter::NoDotAndDotDot);
        connect(mc, &QProcess::finished, this, &Backend::onFinished);
        m_mcCmdLineProvider = new MinecraftCommandLineProvider{this};

        connect(mc, &QProcess::stateChanged, this, [] (QProcess::ProcessState state) { qInfo() << "mc state changed:" << state; });
        connect(mc, &QProcess::errorOccurred, this, [] (QProcess::ProcessError error) { qInfo() << "mc error occured!" << error; });
    }

    void onFinished() {
        m_running = false;
        emit runningChanged();
    }

    QStringList versions() const;
    bool running() const;

public slots:
    void launch(QString version) {
        if (running())
            return;

        const auto cmdLine = m_mcCmdLineProvider->getCommandLine(version);
        if (!cmdLine.has_value())
            return;

        qInfo() << "launching" << version;

        m_running = true;
        emit runningChanged();
        mc->start(cmdLine->first, cmdLine->second);
    }

signals:
    void runningChanged();

private:
    QProcess *mc = new QProcess;
    QStringList m_versions;

    MinecraftCommandLineProvider *m_mcCmdLineProvider;
    bool m_running = false;
};

int Application::run()
{
    QQmlApplicationEngine qml;
    // QLoggingCategory::setFilterRules("randomly.MyLauncher.*=false");
    QLoggingCategory::setFilterRules("randomly.MyLauncher.Config*=false");

    Auth a;
    a.obtainMinecraftToken();

    qmlRegisterType<Backend>("MyLauncher", 1, 0, "Backend");

    qml.loadFromModule("MyLauncherGui", "Main");

    if (qml.rootObjects().isEmpty())
        return EXIT_FAILURE;

    return exec();
}

QStringList Backend::versions() const
{
    return m_versions;
}

bool Backend::running() const
{
    return m_running;
}

} // namespace randomly

int main(int argc, char *argv[])
{
    return randomly::Application{argc, argv}.run();
}

#include "main.moc"
