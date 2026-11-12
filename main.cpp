#include "mainwindow.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    MainWindow w;
    w.setWindowIcon(QIcon(":/new/prefix1/images/logo.ico"));
    w.show();
    return QApplication::exec();
}
