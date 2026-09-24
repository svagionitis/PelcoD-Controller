#include "TacticalMapQuickItem.h"
#include "VideoPlayerController.h"
#include "VideoQuickItem.h"

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QUrl>

int main(int argc, char* argv[])
{
    QGuiApplication app(argc, argv);

    QGuiApplication::setApplicationName(QStringLiteral("PelcoD Tactical Video Player"));
    QGuiApplication::setOrganizationName(QStringLiteral("PelcoD"));
    QGuiApplication::setOrganizationDomain(QStringLiteral("pelcod.local"));
    QGuiApplication::setApplicationVersion(QStringLiteral("1.0.0"));

    QQuickStyle::setStyle(QStringLiteral("Fusion"));

    MappingQt::registerQmlTypes();
    qmlRegisterType<VideoApp::VideoQuickItem>("VideoApp", 1, 0, "VideoItem");
    qmlRegisterType<VideoApp::VideoPlayerController>("VideoApp", 1, 0, "VideoPlayerController");

    QQmlApplicationEngine engine;

    const QUrl url(QStringLiteral("qrc:/qml/Main.qml"));
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreated, &app,
        [url](QObject* obj, const QUrl& objUrl) {
            if (obj == nullptr && url == objUrl) {
                QCoreApplication::exit(-1);
            }
        },
        Qt::QueuedConnection);

    engine.load(url);

    return app.exec();
}
