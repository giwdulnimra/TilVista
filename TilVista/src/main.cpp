#include "ui/mainwindow.h"
#include "core/config.h"

#include <QApplication>

// TV_APPVERSION and TV_VERSION injected by CMake via target_compile_definitions
#ifndef TV_APPVERSION
#  define TV_APPVERSION "v0_0530"
#endif
#ifndef TV_VERSION
#  define TV_VERSION "0.5.30"
#endif

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("TilVista");
    app.setApplicationVersion(TV_VERSION);
    app.setOrganizationName("Ludwig, Armin");

    Config::load();

    MainWindow w;
    w.show();
    return app.exec();
}
