/// @file main.cpp
/// @brief Entry point for Pelco-D Controller Qt 6 GUI application.

#include "MainWindow.h"

#include <QApplication>
#include <QFile>
#include <glog/logging.h>

int main(int argc, char* argv[])
{
    google::InitGoogleLogging(argv[0]);
    FLAGS_logtostderr = 1;

    QApplication app(argc, argv);
    app.setApplicationName("PelcoDController");
    app.setApplicationDisplayName("Pelco-D Device Controller");

    // Load modern dark theme
    QFile styleFile(":/resources/ThemeDark.qss");
    if (styleFile.open(QFile::ReadOnly | QFile::Text)) {
        const QString style = QLatin1String(styleFile.readAll());
        app.setStyleSheet(style);
        styleFile.close();
    }

    PelcoDApp::MainWindow mainWindow;
    mainWindow.show();

    return app.exec();
}
