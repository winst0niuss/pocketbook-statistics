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
#include <QtGlobal>

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

/* Qt writes its own diagnostics to stderr, and this device has none: a QML
 * error is printed where nobody can read it, and the app then closes itself
 * with no explanation anywhere. Route the warnings into app.log for the one
 * stretch where that matters — building the scene. */
void logQtMessage(QtMsgType type, const QMessageLogContext &, const QString &text)
{
    if (type == QtDebugMsg || type == QtInfoMsg)
        return;
    updateLog(QStringLiteral("qml: ") + text);
}

} // namespace

int main(int argc, char *argv[])
{
    /* Daemon mode before any Qt: pure C loop, no UI. */
    if (argc > 1 && std::strcmp(argv[1], "--daemon") == 0)
        return run_daemon();

    /* Before anything else, and before any Qt call: on a firmware this was not
     * built against, the process can die at the first missing symbol, and a
     * start line written later would never be reached — leaving a report with
     * no log at all indistinguishable from one where the app never ran. The Qt
     * the reader supplies is half of that answer, so it goes in the same line;
     * qVersion() is the runtime's, not the 6.8.2 this was compiled against. */
    updateLog(QStringLiteral("app: start, version " APP_VERSION ", Qt %1")
                  .arg(QString::fromLatin1(qVersion())));

    selectPlatformPlugin();
    QCoreApplication::setSetuidAllowed(true);

    const ScreenSize screen = openInkViewScreen();
    /* The layout is built from these three numbers, so a screen report from an
     * unknown reader is worth the line. */
    updateLog(QStringLiteral("app: screen %1x%2, panel %3")
                  .arg(screen.width).arg(screen.height).arg(screen.panelHeight));

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

    const QString fontFamily = inkViewFontFamily();
    if (!fontFamily.isEmpty())
        QGuiApplication::setFont(QFont(fontFamily));

    StatsBridge stats;
    Updater updater;
    Shim shim;
    /* An app update ships a new shim; nothing else would ever install it. */
    shim.refresh();
    /* And on a device that has never had one, put it in: nothing of ours starts
     * at boot, so without it the day is measured only while the app happens to
     * be open. */
    shim.enableByDefault();
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

    QtMessageHandler previous = qInstallMessageHandler(logQtMessage);
    engine.load(QUrl(QString::fromUtf8(kSceneUrl)));
    if (engine.rootObjects().isEmpty()) {
        /* The scene failed to instantiate — a QML mistake the lint gate let
         * through, or a firmware whose com.pocketbook.controls is not the one
         * this was written against. There is no console on this device, so
         * without the handler above the app simply never opens and says
         * nothing; the lines before this one name the type that failed. */
        updateLog(QStringLiteral("app: QML scene is empty, exiting"));
        return 1;
    }
    /* Off again once the scene stands: what Qt has to say after that is noise,
     * and the log is 64 KB for a week. */
    qInstallMessageHandler(previous);
    return app.exec();
}
