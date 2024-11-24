#include <QApplication>
#include "mainwindow.h"
#include <QMenuBar>
#include <string>


int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    // determine the startup config file...
    std::string config_file = a.applicationDirPath().toStdString() + "/SerialPortLSL.cfg";
    for (int k=1; k<argc; k++)
        if (std::string(argv[k]) == "-c" || std::string(argv[k]) == "--config")
            config_file = argv[k+1];

    MainWindow w(0, config_file);
    w.show();
    
    return a.exec();
}
