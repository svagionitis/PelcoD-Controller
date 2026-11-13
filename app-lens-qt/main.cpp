/// @file main.cpp
/// @brief Application entry point for the Qt6 QML Motorized Lens Controller Application.

#include <cstdint>

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QUrl>

#include "LensConfigModel.h"
#include "LensPlayerController.h"
#include "LensVideoQuickItem.h"
#include "PresentationTypes.h"

int main(int argc, char* argv[])
{
    QGuiApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("LensAppQt"));
    app.setOrganizationName(QStringLiteral("PelcoD"));
    app.setOrganizationDomain(QStringLiteral("pelcod.org"));

    QQuickStyle::setStyle(QStringLiteral("Basic"));

    // Register C++ ViewModel and QuickItem components into QML
    qmlRegisterType<LensApp::LensPlayerController>("LensApp", 1, 0, "LensPlayerController");
    qmlRegisterType<LensApp::LensVideoQuickItem>("LensApp", 1, 0, "LensVideoQuickItem");
    qmlRegisterUncreatableType<LensApp::LensConfigModel>(
        "LensApp", 1, 0, "LensConfigModel", QStringLiteral("Access via LensPlayerController"));
    qmlRegisterUncreatableMetaObject(LensApp::PresentationTypes::staticMetaObject, "LensApp", 1, 0, "PresentationTypes",
        QStringLiteral("PresentationTypes holds scoped enums only"));

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
