cmake_minimum_required(VERSION 3.16)

project(MyLauncher VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 23)

find_package(Qt6 REQUIRED COMPONENTS Core Quick NetworkAuth)
find_package(LibArchive REQUIRED)

qt_standard_project_setup(REQUIRES 6.5)

qt_policy(SET QTP0004 OLD)

qt_add_library (
    MyLauncherCore SHARED
    src/minecraftcommandlineprovider.h src/minecraftcommandlineprovider.cpp
    src/config.h src/config.cpp
    src/downloader.h src/downloader.cpp
    src/auth.h src/auth.cpp
)

qt_add_executable(MyLauncher
    src/main.cpp
)

qt_add_qml_module(MyLauncher
    URI MyLauncher
    VERSION 1.0
    QML_FILES
        qml/Main.qml

    RESOURCES ReadMe.md Roadmap.md
)

add_custom_target(Documentation SOURCES
    versionInfo.json
)

# versioning in C++
add_compile_definitions(LAUNCHER_VERSION=${PROJECT_VERSION})

# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
set_target_properties(MyLauncher PROPERTIES
#    MACOSX_BUNDLE_GUI_IDENTIFIER com.example.MyLauncher
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

target_link_libraries(MyLauncherCore
    PRIVATE Qt6::Core Qt6::NetworkAuth
    LibArchive::LibArchive
)

target_link_libraries(MyLauncher
    PRIVATE Qt6::Quick
)

include(GNUInstallDirs)
install(TARGETS MyLauncher
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
