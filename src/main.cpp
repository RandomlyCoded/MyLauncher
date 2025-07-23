#include "minecraftcommandlineprovider.h"

#include "auth.h"

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

int Application::run()
{
    QQmlApplicationEngine qml;
    // QLoggingCategory::setFilterRules("randomly.MyLauncher.*=false");
    QLoggingCategory::setFilterRules("randomly.MyLauncher.Config*=false");

    Auth a;
    a.obtainMinecraftToken();

    MinecraftCommandLineProvider p;

    auto cmdLine = p.getCommandLine("fabric-loader-0.15.11-1.18.2").value();
    // auto cmdLine = p.getCommandLine("myver").value();
    qInfo() << "\n\n" << cmdLine.second << "\n\n";

    QProcess mc;
    qInfo() << QProcess::execute(cmdLine.first, cmdLine.second);

    qml.loadFromModule("MyLauncher", "Main");

    if (qml.rootObjects().isEmpty())
        return EXIT_FAILURE;

    return exec();
}

} // namespace randomly

int main(int argc, char *argv[])
{
    return randomly::Application{argc, argv}.run();
}
