#include "src/minecraftcommandlineprovider.h"

#include <QGuiApplication>
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

    MinecraftCommandLineProvider p;
    qInfo() << p.getCommandLine("fabric-loader-0.15.11-1.18.2");

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
