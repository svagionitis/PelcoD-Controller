/// @file main.cpp
/// @brief Application entry point for the Qt QML Video Stabilizer Testing Application.

#include <cstdint>

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QUrl>

#include "StabilizerPlayerController.h"
#include "StabilizerQuickItem.h"

int main(int argc, char* argv[])
{
    QGuiApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("StabilizerAppQt"));
    app.setOrganizationName(QStringLiteral("PelcoD"));
    app.setOrganizationDomain(QStringLiteral("pelcod.org"));

    QQuickStyle::setStyle(QStringLiteral("Basic"));

    // Register C++ ViewModel and QuickItem components into QML
    qmlRegisterType<StabilizerApp::StabilizerPlayerController>("StabilizerApp", 1, 0, "StabilizerPlayerController");
    qmlRegisterType<StabilizerApp::StabilizerQuickItem>("StabilizerApp", 1, 0, "StabilizerQuickItem");

    QQmlApplicationEngine engine;
    const QUrl url(QStringLiteral("qrc:/qml/Main.qml"));

    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreated, &app,
        [url](QObject* obj, const QUrl& objUrl) {
            if ((obj == nullptr) && (url == objUrl)) {
                QGuiApplication::exit(-1);
            }
        },
        Qt::QueuedConnection);

    engine.load(url);

    return app.exec();
}
