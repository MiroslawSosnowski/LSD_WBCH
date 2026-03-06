#include <QApplication>
#include "main_window.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("LS-DYNA Workbench");
    app.setOrganizationName("LSDYNA_WB");

    MainWindow w;
    w.show();

    return app.exec();
}
