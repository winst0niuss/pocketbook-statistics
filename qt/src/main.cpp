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
#include <QStringList>
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

/* A build with a suffix on its version is a build somebody has been asked to
 * test, and the question asked of it is always the same one: how far did it
 * get. Marks through startup earn their lines there; in a release they would
 * be half a dozen lines of "nothing wrong" a launch, drowning the week's
 * signal. */
bool verboseStartup()
{
    return std::strchr(APP_VERSION, '-') != nullptr;
}

void mark(const QString &stage)
{
    if (verboseStartup())
        updateLog(QStringLiteral("app: ") + stage);
}

/* The QPA plugin is loaded by name, and a name this firmware does not carry is
 * a qFatal inside the QGuiApplication constructor — on a device with no console
 * the app dies there having said nothing, which looks exactly like never having
 * been started. So ask the directory first: what is in it is what can be
 * loaded. `pocketbook2` where it exists, anything else PocketBook's rather than
 * nothing, and Qt's own choice when the directory cannot be read. */
QByteArray choosePlatform()
{
    QStringList names;
    const QDir dir(QString::fromLatin1(kPluginPath) + QStringLiteral("/platforms"));
    const QStringList files = dir.entryList({QStringLiteral("lib*.so")}, QDir::Files);
    for (const QString &file : files)
        names += file.mid(3, file.size() - 6);

    const QString preferred = QString::fromLatin1(kPlatformName);
    if (names.contains(preferred))
        return QByteArray(kPlatformName);

    /* Not a mark: a reader without the plugin this was built for is the whole
     * report, and it is worth a line in any build. */
    updateLog(QStringLiteral("app: no %1 platform plugin, found [%2]")
                  .arg(preferred, names.join(QStringLiteral(", "))));

    for (const QString &name : names) {
        if (name.startsWith(QLatin1String("pocketbook")))
            return name.toUtf8();
    }
    return {};
}

void selectPlatformPlugin()
{
    if (qEnvironmentVariableIsEmpty("QT_PLUGIN_PATH"))
        qputenv("QT_PLUGIN_PATH", QByteArray(kPluginPath));
    if (!qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))
        return;
    const QByteArray platform = choosePlatform();
    if (!platform.isEmpty())
        qputenv("QT_QPA_PLATFORM", platform);
}

/* Qt writes its own diagnostics to stderr, and this device has none: a QML
 * error is printed where nobody can read it, and the app then closes itself
 * with no explanation anywhere. Route the warnings into app.log for the whole
 * of startup — the QML scene is not the only thing that can fail there, and a
 * platform plugin that will not load is a qFatal in the QGuiApplication
 * constructor, which is to say the last thing this process ever does. */
void logQtMessage(QtMsgType type, const QMessageLogContext &, const QString &text)
{
    if (type == QtDebugMsg || type == QtInfoMsg)
        return;
    updateLog(QStringLiteral("qt: ") + text);
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

    /* From here to the scene, everything Qt has to say goes into the log. */
    QtMessageHandler previous = qInstallMessageHandler(logQtMessage);

    selectPlatformPlugin();
    QCoreApplication::setSetuidAllowed(true);

    const ScreenSize screen = openInkViewScreen();
    /* The layout is built from these three numbers, so a screen report from an
     * unknown reader is worth the line. */
    updateLog(QStringLiteral("app: screen %1x%2, panel %3")
                  .arg(screen.width).arg(screen.height).arg(screen.panelHeight));

    // Register the launcher icon on first run (idempotent, no-op afterwards).
    ensureRegistered();
    mark(QStringLiteral("launcher registered"));

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
    mark(QStringLiteral("Qt up on ") + QGuiApplication::platformName());

    const QString fontFamily = inkViewFontFamily();
    if (!fontFamily.isEmpty())
        QGuiApplication::setFont(QFont(fontFamily));
    mark(QStringLiteral("font ") + (fontFamily.isEmpty() ? QStringLiteral("(Qt default)")
                                                         : fontFamily));

    StatsBridge stats;
    Updater updater;
    Shim shim;
    /* An app update ships a new shim; nothing else would ever install it. */
    shim.refresh();
    /* And on a device that has never had one, put it in: nothing of ours starts
     * at boot, so without it the day is measured only while the app happens to
     * be open. */
    shim.enableByDefault();
    mark(QStringLiteral("shim checked"));
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

    mark(QStringLiteral("loading scene"));
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
    mark(QStringLiteral("scene up"));
    return app.exec();
}
