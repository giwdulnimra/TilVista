#include "ui/mainwindow.h"
#include "core/config.h"

#include <QApplication>

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("TilVista");
    app.setApplicationVersion("0.5.10");
    app.setOrganizationName("Ludwig, Armin");

    Config::load();   // load tilvista_config.json once at startup

    MainWindow w;
    w.show();
    return app.exec();
}
