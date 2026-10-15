/// @file main.cpp
/// @brief Entry point for ONVIF Device Manager & Server Qt 6 GUI application.

#include "MainWindow.h"

#include <QApplication>
#include <QFile>
#include <glog/logging.h>

int main(int argc, char* argv[])
{
    google::InitGoogleLogging(argv[0]);
    FLAGS_logtostderr = 1;

    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("OnvifController"));
    app.setApplicationDisplayName(QStringLiteral("ONVIF Profile S/T/G Control Suite"));

    // Load modern dark theme
    QFile styleFile(QStringLiteral(":/resources/ThemeDark.qss"));
    if (styleFile.open(QFile::ReadOnly | QFile::Text)) {
        const QString style = QLatin1String(styleFile.readAll());
        app.setStyleSheet(style);
        styleFile.close();
    }

    OnvifApp::MainWindow mainWindow;
    mainWindow.show();

    return app.exec();
}
