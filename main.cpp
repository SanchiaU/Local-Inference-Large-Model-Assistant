#include "mainwindow.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    QCoreApplication::setOrganizationName(QStringLiteral("LocalLlamacpp"));
    QCoreApplication::setApplicationName(QStringLiteral("LocalLlamacpp"));

    QApplication a(argc, argv);
    MainWindow w;
    w.setWindowIcon(QIcon(":/new/prefix1/images/logo.ico"));
    w.show();
    return QApplication::exec();
}
