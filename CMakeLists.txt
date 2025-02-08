cmake_minimum_required(VERSION 3.16)

project(MyLauncher VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 23)

find_package(Qt6 REQUIRED COMPONENTS Quick)

qt_standard_project_setup(REQUIRES 6.5)

qt_policy(SET QTP0004 OLD)

qt_add_executable(appMyLauncher
    src/main.cpp
)

qt_add_qml_module(appMyLauncher
    URI MyLauncher
    VERSION 1.0
    QML_FILES
        qml/Main.qml

    SOURCES src/minecraftcommandlineprovider.h src/minecraftcommandlineprovider.cpp
    SOURCES src/config.h src/config.cpp
    RESOURCES ReadMe.md Roadmap.md
)

# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
set_target_properties(appMyLauncher PROPERTIES
#    MACOSX_BUNDLE_GUI_IDENTIFIER com.example.appMyLauncher
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

target_link_libraries(appMyLauncher
    PRIVATE Qt6::Quick
)

include(GNUInstallDirs)
install(TARGETS appMyLauncher
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
