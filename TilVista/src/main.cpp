#include "ui/mainwindow.h"
#include "core/config.h"
#include <QApplication>
#include <QIcon>

#ifndef TV_APPVERSION_DISPLAY
#  define TV_APPVERSION_DISPLAY "v-.--.--"
#endif
#ifndef TV_SEMVER
#  define TV_SEMVER "-.--.--"
#endif

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("TilVista");
    app.setApplicationVersion(TV_SEMVER);
    app.setOrganizationName("Ludwig, Armin");

    const QIcon appIcon(":/icons/tilvista.ico");
    if (!appIcon.isNull())
        app.setWindowIcon(appIcon);

    Config::load();
    MainWindow w;
    w.show();
    return app.exec();
}
