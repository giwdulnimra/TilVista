#include "ui/mainwindow.h"
#include <QApplication>

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("TilVista");
    app.setApplicationVersion("0.4.0");
    app.setOrganizationName("Ludwig, Armin");

    MainWindow w;
    w.show();
    return app.exec();
}
