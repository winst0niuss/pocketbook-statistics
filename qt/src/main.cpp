#include <cstring>

extern "C" {
#include "daemon.h"
}

#include <QByteArray>
#include <QDir>
#include <QFont>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QString>
#include <QUrl>

#include "inkview_bridge.h"
#include "installer.h"
#include "update_log.h"
#include "stats_bridge.h"
#include "shim.h"
#include "updater.h"

namespace {

constexpr const char *kPluginPath = "/ebrmain/plugins";
constexpr const char *kQmlPath = "/ebrmain/qml";
constexpr const char *kPlatformName = "pocketbook2";
constexpr const char *kSceneUrl = "qrc:/main.qml";

void selectPlatformPlugin()
{
    if (qEnvironmentVariableIsEmpty("QT_PLUGIN_PATH"))
        qputenv("QT_PLUGIN_PATH", QByteArray(kPluginPath));
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))
        qputenv("QT_QPA_PLATFORM", QByteArray(kPlatformName));
}

} // namespace

int main(int argc, char *argv[])
{
    /* Daemon mode before any Qt: pure C loop, no UI. */
    if (argc > 1 && std::strcmp(argv[1], "--daemon") == 0)
        return run_daemon();

    selectPlatformPlugin();
    QCoreApplication::setSetuidAllowed(true);

    const ScreenSize screen = openInkViewScreen();

    // Register the launcher icon on first run (idempotent, no-op afterwards).
    ensureRegistered();

    /* Compiling the QML takes ~3 s of the 3.5 s this app needs to appear, and
     * it happens on every launch: Qt disk-caches compiled QML, but not when it
     * comes out of a resource — there it assumes reading is already cheap and
     * skips the cache. On this hardware that assumption is wrong, so force it
     * on and point it at our own directory (the default lives under a HOME
     * this firmware may not give us). A cache that does not match the engine
     * or the source is discarded and rebuilt, so the worst case is the speed
     * we have now. */
    QDir().mkpath(QStringLiteral(STATS_DIR "/qmlcache"));
    qputenv("QML_DISK_CACHE_PATH", STATS_DIR "/qmlcache");
    qputenv("QML_FORCE_DISK_CACHE", "1");

    QQuickWindow::setGraphicsApi(QSGRendererInterface::Software);

    QGuiApplication app(argc, argv);
    updateLog(QStringLiteral("app: start, version " APP_VERSION));

    const QString fontFamily = inkViewFontFamily();
    if (!fontFamily.isEmpty())
        QGuiApplication::setFont(QFont(fontFamily));

    StatsBridge stats;
    Updater updater;
    Shim shim;
    /* An app update ships a new shim; nothing else would ever install it. */
    shim.refresh();
    spawn_daemon(QGuiApplication::applicationFilePath().toUtf8().constData());

    QQmlApplicationEngine engine;
    engine.addImportPath(QString::fromUtf8(kQmlPath));
    engine.rootContext()->setContextProperty(QStringLiteral("stats"), &stats);
    engine.rootContext()->setContextProperty(QStringLiteral("updater"), &updater);
    engine.rootContext()->setContextProperty(QStringLiteral("shim"), &shim);
    engine.rootContext()->setContextProperty(QStringLiteral("deviceLang"),
                                              inkViewLang());
    engine.rootContext()->setContextProperty(QStringLiteral("screenW"), screen.width);
    engine.rootContext()->setContextProperty(QStringLiteral("screenH"), screen.height);
    engine.rootContext()->setContextProperty(QStringLiteral("panelH"), screen.panelHeight);

    engine.load(QUrl(QString::fromUtf8(kSceneUrl)));
    if (engine.rootObjects().isEmpty()) {
        /* The scene failed to instantiate — a QML mistake the lint gate let
         * through. There is no console on this device, so without this line
         * the app simply never opens and says nothing. */
        updateLog(QStringLiteral("app: QML scene is empty, exiting"));
        return 1;
    }
    return app.exec();
}
