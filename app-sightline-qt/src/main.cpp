/// @file main.cpp
/// @brief Main entry point for the Sightline SLA Qt QML Control Application.

#include "SightlineQmlBridge.h"
#include "TrackListModel.h"
#include "TrafficLogModel.h"

#include <QGuiApplication>
#include <QPalette>
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

    // Apply Fusion style matching app-video-qt
    QQuickStyle::setStyle(QStringLiteral("Fusion"));

    QGuiApplication app(argc, argv);

    // Apply global tactical dark palette to ensure all Fusion controls render dark
    QPalette darkPalette {};
    darkPalette.setColor(QPalette::Window, QColor("#0e1014"));
    darkPalette.setColor(QPalette::WindowText, QColor("#f0f4fc"));
    darkPalette.setColor(QPalette::Base, QColor("#141720"));
    darkPalette.setColor(QPalette::AlternateBase, QColor("#1e2230"));
    darkPalette.setColor(QPalette::ToolTipBase, QColor("#181b24"));
    darkPalette.setColor(QPalette::ToolTipText, QColor("#f0f4fc"));
    darkPalette.setColor(QPalette::Text, QColor("#f0f4fc"));
    darkPalette.setColor(QPalette::Button, QColor("#181b24"));
    darkPalette.setColor(QPalette::ButtonText, QColor("#f0f4fc"));
    darkPalette.setColor(QPalette::BrightText, QColor("#ff1744"));
    darkPalette.setColor(QPalette::Highlight, QColor("#00e5ff"));
    darkPalette.setColor(QPalette::HighlightedText, QColor("#0e1014"));
    app.setPalette(darkPalette);

    // Register custom C++ types with QML type system
    qmlRegisterUncreatableType<TrackListModel>(
        "Sightline", 1, 0, "TrackListModel", QStringLiteral("TrackListModel is instantiated by SightlineQmlBridge"));
    qmlRegisterUncreatableType<TrafficLogModel>(
        "Sightline", 1, 0, "TrafficLogModel", QStringLiteral("TrafficLogModel is instantiated by SightlineQmlBridge"));

    QQmlApplicationEngine engine {};

    auto bridge = std::make_unique<SightlineQmlBridge>(&app);
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
