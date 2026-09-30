/// @file main.cpp
/// @brief Main entry point for the Sightline SLA Qt QML Control Application.

#include "SightlineQmlBridge.h"
#include "TrackListModel.h"
#include "TrafficLogModel.h"

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <memory>

/// @brief Application main function.
/// @param argc Argument count.
/// @param argv Argument vector.
/// @return Process exit code.
int main(int argc, char* argv[])
{
    QGuiApplication::setApplicationName(QStringLiteral("Sightline SLA Controller"));
    QGuiApplication::setOrganizationName(QStringLiteral("Sightline Intelligence"));
    QGuiApplication::setApplicationVersion(QStringLiteral("3.11.6"));

    QGuiApplication app(argc, argv);

    // Register custom C++ types with QML type system
    qmlRegisterUncreatableType<TrackListModel>(
        "Sightline", 1, 0, "TrackListModel", QStringLiteral("TrackListModel is instantiated by SightlineQmlBridge"));
    qmlRegisterUncreatableType<TrafficLogModel>(
        "Sightline", 1, 0, "TrafficLogModel", QStringLiteral("TrafficLogModel is instantiated by SightlineQmlBridge"));

    QQmlApplicationEngine engine {};

    auto bridge = std::make_unique<SightlineQmlBridge>();
    engine.rootContext()->setContextProperty(QStringLiteral("bridge"), bridge.get());

    const QUrl url(QStringLiteral("qrc:/qml/Main.qml"));
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreated, &app,
        [url](QObject* obj, const QUrl& objUrl) {
            if (!obj && url == objUrl) {
                QCoreApplication::exit(-1);
            }
        },
        Qt::QueuedConnection);

    engine.load(url);

    return app.exec();
}
