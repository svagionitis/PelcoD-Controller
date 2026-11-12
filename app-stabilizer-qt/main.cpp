/// @file main.cpp
/// @brief Application entry point for the Qt QML Video Stabilizer Testing Application.

#include <cstdint>

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QUrl>

#include "PresetManager.h"
#include "StabilizerConfigModel.h"
#include "StabilizerPlayerController.h"
#include "StabilizerQuickItem.h"
#include "TelemetryRingBuffer.h"

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
    qmlRegisterUncreatableType<StabilizerApp::StabilizerConfigModel>(
        "StabilizerApp", 1, 0, "StabilizerConfigModel", QStringLiteral("Access via StabilizerPlayerController"));
    qmlRegisterUncreatableType<StabilizerApp::TelemetryRingBuffer>(
        "StabilizerApp", 1, 0, "TelemetryRingBuffer", QStringLiteral("Access via StabilizerPlayerController"));
    qmlRegisterUncreatableType<StabilizerApp::PresetManager>(
        "StabilizerApp", 1, 0, "PresetManager", QStringLiteral("Access via StabilizerPlayerController"));

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
