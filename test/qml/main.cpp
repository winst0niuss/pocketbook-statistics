/* Runs the app's own QML against the real bridge and a device in a temporary
 * directory.
 *
 * The screens import com.pocketbook.controls, which exists only in the reader's
 * firmware, so nothing here could be instantiated on a host until the stub
 * module beside this file. What that buys: the tabs are built, refreshed and
 * measured on every push — the arithmetic that put a figures row 21 px past the
 * screen edge, and the property clashes that make a component fail to
 * instantiate and the app simply not open, are both visible here rather than on
 * the device.
 *
 * `stats` is the real StatsBridge over a seeded database, not a mock: the shape
 * of those maps is exactly what the screens have to survive.
 */
#include <memory>

#include <QDate>
#include <QDateTime>
#include <QPair>
#include <QQmlContext>
#include <QQmlEngine>
#include <QtQuickTest>

#include "fixtures.h"
#include "shim.h"
#include "stats_bridge.h"
#include "updater.h"

namespace {

qint64 noonOn(const QDate &day)
{
    return QDateTime(day, QTime(12, 0)).toSecsSinceEpoch();
}

} // namespace

class Setup : public QObject {
    Q_OBJECT

public:
    Setup()
    {
        createExplorer(device_.explorerDb());
        createStats(device_.statsDb());
        const QDate today = QDate::currentDate();
        setMeta(device_.statsDb(), QStringLiteral("tracking_since"),
                noonOn(QDate(today.year(), 1, 1)) - 43200);

        /* Half an hour today and an hour yesterday: a streak of two, an hour
         * and a half in total, and a pace of 60 pages an hour. */
        insertOwnBook(device_.statsDb(), 1, kTitle, QStringLiteral("Autorin"), QString());
        insertSession(device_.statsDb(), {1, noonOn(today), noonOn(today) + 1800, 1800, 30, 0});
        insertSession(device_.statsDb(),
                      {1, noonOn(today.addDays(-1)), noonOn(today.addDays(-1)) + 3600,
                       3600, 60, 0});

        /* A book open in the reader, half read. */
        BookRow b;
        b.id = 1;
        b.title = kTitle;
        b.author = QStringLiteral("Autorin");
        b.opentime = noonOn(today);
        b.positionTs = b.opentime + 1800;
        b.cpage = 100;
        b.npage = 200;
        insertBook(device_.explorerDb(), b);
    }

public slots:
    void qmlEngineAvailable(QQmlEngine *engine)
    {
        /* Built here rather than as members: StatsBridge opens the statistics
         * database in its constructor, and the fixtures above create that file
         * — a bridge constructed first holds a handle to the file the seeding
         * then replaces, and every screen reads zeros. */
        stats_ = std::make_unique<StatsBridge>();
        updater_ = std::make_unique<Updater>();
        shim_ = std::make_unique<Shim>();

        QQmlContext *ctx = engine->rootContext();
        ctx->setContextProperty(QStringLiteral("stats"), stats_.get());
        ctx->setContextProperty(QStringLiteral("updater"), updater_.get());
        ctx->setContextProperty(QStringLiteral("shim"), shim_.get());
        /* The reader's own metrics; the layout tests measure against these. */
        ctx->setContextProperty(QStringLiteral("screenW"), 758);
        ctx->setContextProperty(QStringLiteral("screenH"), 1024);
        ctx->setContextProperty(QStringLiteral("panelH"), 80);
        ctx->setContextProperty(QStringLiteral("deviceLang"), QStringLiteral("ru"));
        /* The firmware's font table. A context property rather than a QML
         * singleton beside the others: its members are Heading2, BodyS and the
         * like, and a QML property name may not begin with a capital. Only the
         * size is ever measurable from here. */
        QVariantMap fonts;
        for (const auto &style : {QPair<const char *, int>{"Heading2", 44},
                                  {"Heading3", 36}, {"Heading4", 30},
                                  {"BodyL", 26}, {"BodyLBold", 26}, {"Body", 24},
                                  {"BodyS", 21}, {"BodyXS", 18}, {"Caption1", 16}})
            fonts[QLatin1String(style.first)] =
                QVariantMap{{QStringLiteral("pixelSize"), style.second}};
        ctx->setContextProperty(QStringLiteral("FontStyles"), fonts);

        /* What the fixtures above put in the database, so a test asserts
         * against one statement of it rather than against repeated literals. */
        QVariantMap fixture;
        fixture[QStringLiteral("title")] = kTitle;
        fixture[QStringLiteral("todaySecs")] = 1800;
        fixture[QStringLiteral("totalHours")] = 1.5;
        fixture[QStringLiteral("pagesPerHour")] = 60;
        fixture[QStringLiteral("streakDays")] = 2;
        fixture[QStringLiteral("percent")] = 50;
        fixture[QStringLiteral("today")] = QDate::currentDate();
        ctx->setContextProperty(QStringLiteral("fixture"), fixture);
    }

private:
    static inline const QString kTitle = QStringLiteral("Der Steppenwolf");

    Device device_;
    std::unique_ptr<StatsBridge> stats_;
    std::unique_ptr<Updater> updater_;
    std::unique_ptr<Shim> shim_;
};

QUICK_TEST_MAIN_WITH_SETUP(qml, Setup)

#include "main.moc"
