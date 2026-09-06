/* Everything the two screens read comes through here: four aggregates, two
 * setters and the way back into the reader. The SQL underneath is the C core's
 * and has its own tests — what is tested here is the shaping on top of it, and
 * the rules that live nowhere else: the hand-set offsets touching two figures
 * and nothing else, the dedupe by title, the cover lookup order that decides
 * whether a finished book still has a thumbnail, and the days a screen must
 * draw as "not recorded" rather than as "not read".
 */
#include <QDate>
#include <QDateTime>
#include <QFile>
#include <QImage>
#include <QTest>
#include <QUrl>
#include <QVariantList>

#include "device_paths.h"
#include "fixtures.h"
#include "inkview_stub.h"
#include "stats_bridge.h"

extern "C" {
#include "daemon.h"
}

namespace {

qint64 noonOn(const QDate &day)
{
    return QDateTime(day, QTime(12, 0)).toSecsSinceEpoch();
}

/* A day of this month that exists whatever day the suite runs on, and is not
 * the first (the tracking marker goes there). */
QDate dayOfThisMonth(int day)
{
    const QDate today = QDate::currentDate();
    return QDate(today.year(), today.month(), day);
}

QVariantMap dayEntry(const QVariantMap &month, int day)
{
    return month.value(QStringLiteral("days")).toList().at(day - 1).toMap();
}

QVariantList booksOn(const QVariantMap &month, int day)
{
    return dayEntry(month, day).value(QStringLiteral("books")).toList();
}

const char *kOurs = "#204080";      /* what a cover from our own cache looks like */
const char *kFirmware = "#ff0000";  /* ...and from the firmware's */

} // namespace

class TestStatsBridge : public QObject {
    Q_OBJECT

private slots:
    void init()
    {
        InkViewStub::reset();
        device_ = new Device;
        createExplorer(device_->explorerDb());
        createStats(device_->statsDb());
        /* Everything this suite writes is inside the current year, so the
         * marker goes to the start of it. */
        setMeta(device_->statsDb(), QStringLiteral("tracking_since"),
                noonOn(QDate(QDate::currentDate().year(), 1, 1)) - 43200);
    }
    void cleanup() { delete device_; device_ = nullptr; }

    void overallReportsWhatWasMeasured();
    void withoutADatabaseEveryAggregateIsEmpty();
    void manualTotalsMoveTwoFiguresAndNothingElse();
    void manualTotalsAreClearedByANegative();
    void currentBookSaysWhatIsOpen();
    void currentBookWithoutOneSaysSo();
    void aFinishedBookHasNoTimeLeft();
    void theBookCardOnlyLeadsBackWhereThereIsAFile();
    void openBookRefusesAPathThatIsGone();
    void monthPutsTheReadingOnTheRightDays();
    void monthMergesTheSameBookUnderItsVariantTitles();
    void monthCarriesFirmwareFinishesEvenBeforeTracking();
    void monthMarksWhereTrackingBegins();
    void yearFeedsTheStreakScreen();
    void coverComesFromOurCacheFirst();
    void coverIsAdoptedFromTheFirmwareCache();
    void aLegacyCoverKeyStillResolves();
    void theBookFileBeatsTheFirmwareCache();
    void theVariantThatStillHasAFileWins();

private:
    Device *device_ = nullptr;
};

/* ---- the Overview ------------------------------------------------------- */

void TestStatsBridge::overallReportsWhatWasMeasured()
{
    const QDate today = QDate::currentDate();
    insertSession(device_->statsDb(),
                  {1, noonOn(today), noonOn(today) + 1800, 1800, 60, 0});
    insertSession(device_->statsDb(),
                  {1, noonOn(today.addDays(-1)), noonOn(today.addDays(-1)) + 3600,
                   3600, 60, 0});
    /* An estimate, not a measurement: it counts toward the total and toward
     * nothing that is divided. */
    insertSession(device_->statsDb(),
                  {1, noonOn(today.addDays(-2)), noonOn(today.addDays(-2)) + 1800,
                   1800, 0, 1});

    StatsBridge stats;
    const QVariantMap m = stats.overall();

    QCOMPARE(m.value(QStringLiteral("todaySecs")).toInt(), 1800);
    QCOMPARE(m.value(QStringLiteral("totalHours")).toDouble(), 2.0);
    /* 120 pages over 90 measured minutes; the recovered half hour carries no
     * pages and must not dilute the pace. */
    QCOMPARE(qRound(m.value(QStringLiteral("pagesPerHour")).toDouble()), 80);
    /* Three days, estimate included: a day is read or it is not, and the
     * recovered row is exactly as good a witness of that as the totals treat
     * it. It is the pace above that it must stay out of. */
    QCOMPARE(m.value(QStringLiteral("streakDays")).toInt(), 3);
    QCOMPARE(m.value(QStringLiteral("booksFinished")).toInt(), 0);
}

/* A database that would not open leaves the screens with nothing to say. Zeros
 * would read as "you have not read anything", which is a different claim. */
void TestStatsBridge::withoutADatabaseEveryAggregateIsEmpty()
{
    qputenv("POCKETBOOK_STATISTICS_DB", device_->at(QStringLiteral("/nowhere/x.db")).toUtf8());
    StatsBridge stats;
    qputenv("POCKETBOOK_STATISTICS_DB", device_->statsDb().toUtf8());

    QVERIFY(stats.overall().isEmpty());
    QVERIFY(stats.currentBook().isEmpty());
    QVERIFY(stats.month(QDate::currentDate().year(), 1).isEmpty());
    QVERIFY(stats.year(QDate::currentDate().year()).isEmpty());
    /* And the setters do nothing rather than crashing on a null handle. */
    stats.setTotalHours(10);
    stats.setBooksFinished(10);
}

/* Reading done before the app was installed is invisible to it. The two
 * all-time cards can be set by hand; the difference lives in meta, so later
 * reading still adds to them — and today's minutes, the pace, the calendar and
 * the streak stay measurements. */
void TestStatsBridge::manualTotalsMoveTwoFiguresAndNothingElse()
{
    const QDate today = QDate::currentDate();
    insertSession(device_->statsDb(),
                  {1, noonOn(today), noonOn(today) + 3600, 3600, 120, 0});
    insertBook(device_->explorerDb(),
               [] { BookRow b; b.id = 1; b.title = QStringLiteral("Done");
                    b.completed = 1; b.completedTs = 1000; return b; }());

    StatsBridge stats;
    const QVariantMap before = stats.overall();
    QCOMPARE(before.value(QStringLiteral("totalHours")).toDouble(), 1.0);
    QCOMPARE(before.value(QStringLiteral("booksFinished")).toInt(), 1);

    stats.setTotalHours(101.0);
    stats.setBooksFinished(27);

    const QVariantMap after = stats.overall();
    QCOMPARE(after.value(QStringLiteral("totalHours")).toDouble(), 101.0);
    QCOMPARE(after.value(QStringLiteral("booksFinished")).toInt(), 27);
    /* Not measurements — these must not have moved. */
    QCOMPARE(after.value(QStringLiteral("todaySecs")), before.value(QStringLiteral("todaySecs")));
    QCOMPARE(after.value(QStringLiteral("pagesPerHour")),
             before.value(QStringLiteral("pagesPerHour")));
    QCOMPARE(after.value(QStringLiteral("streakDays")),
             before.value(QStringLiteral("streakDays")));
    QCOMPARE(stats.year(today.year()).value(QStringLiteral("readDays")).toInt(), 1);

    /* An offset, not a figure: another hour of reading still adds to it. */
    insertSession(device_->statsDb(),
                  {1, noonOn(today) + 7200, noonOn(today) + 10800, 3600, 0, 0});
    QCOMPARE(stats.overall().value(QStringLiteral("totalHours")).toDouble(), 102.0);
}

void TestStatsBridge::manualTotalsAreClearedByANegative()
{
    insertSession(device_->statsDb(),
                  {1, noonOn(QDate::currentDate()), noonOn(QDate::currentDate()) + 3600,
                   3600, 0, 0});
    StatsBridge stats;
    stats.setTotalHours(50);
    stats.setBooksFinished(9);
    QCOMPARE(stats.overall().value(QStringLiteral("totalHours")).toDouble(), 50.0);

    stats.setTotalHours(-1);
    stats.setBooksFinished(-1);

    QCOMPARE(stats.overall().value(QStringLiteral("totalHours")).toDouble(), 1.0);
    QCOMPARE(stats.overall().value(QStringLiteral("booksFinished")).toInt(), 0);
}

/* ---- the current book --------------------------------------------------- */

void TestStatsBridge::currentBookSaysWhatIsOpen()
{
    BookRow b;
    b.id = 4;
    b.title = QStringLiteral("Der Steppenwolf");
    b.author = QStringLiteral("Hesse");
    b.opentime = noonOn(QDate::currentDate());
    b.positionTs = b.opentime + 600;
    b.cpage = 50;
    b.npage = 200;
    insertBook(device_->explorerDb(), b);
    /* 60 pages an hour, so the 150 pages left are two and a half hours. */
    insertSession(device_->statsDb(),
                  {4, noonOn(QDate::currentDate().addDays(-1)),
                   noonOn(QDate::currentDate().addDays(-1)) + 3600, 3600, 60, 0});

    StatsBridge stats;
    const QVariantMap m = stats.currentBook();

    QVERIFY(m.value(QStringLiteral("ok")).toBool());
    QCOMPARE(m.value(QStringLiteral("title")).toString(), b.title);
    QCOMPARE(m.value(QStringLiteral("author")).toString(), b.author);
    QCOMPARE(m.value(QStringLiteral("percent")).toInt(), 25);
    QCOMPARE(m.value(QStringLiteral("leftSecs")).toLongLong(), 9000LL);
}

void TestStatsBridge::currentBookWithoutOneSaysSo()
{
    StatsBridge stats;
    QCOMPARE(stats.currentBook().value(QStringLiteral("ok")).toBool(), false);
}

void TestStatsBridge::aFinishedBookHasNoTimeLeft()
{
    BookRow b;
    b.id = 4;
    b.title = QStringLiteral("Fertig");
    b.opentime = noonOn(QDate::currentDate());
    b.positionTs = b.opentime + 600;
    b.cpage = 190;
    b.npage = 200;
    b.completed = 1;
    insertBook(device_->explorerDb(), b);
    insertSession(device_->statsDb(),
                  {4, noonOn(QDate::currentDate()), noonOn(QDate::currentDate()) + 3600,
                   3600, 60, 0});

    StatsBridge stats;
    const QVariantMap m = stats.currentBook();

    QCOMPARE(m.value(QStringLiteral("percent")).toInt(), 100);
    QCOMPARE(m.value(QStringLiteral("leftSecs")).toLongLong(), 0LL);
}

/* The card is a shortcut back into the reader, and only where there is still
 * something to open: `files` is an inventory of what exists right now, and a
 * cloud row carries no folder at all. */
void TestStatsBridge::theBookCardOnlyLeadsBackWhereThereIsAFile()
{
    device_->write(QStringLiteral("/books/open.epub"), QByteArrayLiteral("not really an epub"));
    BookRow b;
    b.id = 4;
    b.title = QStringLiteral("Vorhanden");
    b.opentime = noonOn(QDate::currentDate());
    b.positionTs = b.opentime + 600;
    b.npage = 100;
    b.folder = device_->at(QStringLiteral("/books"));
    b.filename = QStringLiteral("open.epub");
    insertBook(device_->explorerDb(), b);

    StatsBridge stats;
    const QString path = stats.currentBook().value(QStringLiteral("filePath")).toString();
    QCOMPARE(path, device_->at(QStringLiteral("/books/open.epub")));

    QVERIFY(stats.openBook(path));
    QCOMPARE(InkViewStub::openedBooks, QList<QString>{path});

    /* A firmware that exports no OpenBook at all: the file was handed over and
     * nothing happened, so the screen has to stay where it is. */
    InkViewStub::openBookSupported = false;
    QVERIFY(!stats.openBook(path));
    InkViewStub::openBookSupported = true;

    /* Deleted over USB while the app had the map: the path is checked again. */
    QVERIFY(QFile::remove(path));
    QVERIFY(stats.currentBook().value(QStringLiteral("filePath")).toString().isEmpty());
}

void TestStatsBridge::openBookRefusesAPathThatIsGone()
{
    StatsBridge stats;
    QVERIFY(!stats.openBook(QString()));
    QVERIFY(!stats.openBook(device_->at(QStringLiteral("/books/never.epub"))));
    QVERIFY(InkViewStub::openedBooks.isEmpty());
}

/* ---- the calendar ------------------------------------------------------- */

void TestStatsBridge::monthPutsTheReadingOnTheRightDays()
{
    const QDate first = dayOfThisMonth(1);
    insertOwnBook(device_->statsDb(), 1, QStringLiteral("Buch A"),
                  QStringLiteral("Autorin"), QString());
    insertOwnBook(device_->statsDb(), 2, QStringLiteral("Buch B"),
                  QStringLiteral("Autor"), QString());
    insertSession(device_->statsDb(),
                  {1, noonOn(dayOfThisMonth(5)), noonOn(dayOfThisMonth(5)) + 1800, 1800, 30, 0});
    insertSession(device_->statsDb(),
                  {2, noonOn(dayOfThisMonth(5)), noonOn(dayOfThisMonth(5)) + 600, 600, 10, 0});
    /* Under a minute on the day: not reading, and not a book on the calendar. */
    insertSession(device_->statsDb(),
                  {1, noonOn(dayOfThisMonth(6)), noonOn(dayOfThisMonth(6)) + 30, 30, 1, 0});

    StatsBridge stats;
    const QVariantMap m = stats.month(first.year(), first.month());

    QCOMPARE(m.value(QStringLiteral("ndays")).toInt(), first.daysInMonth());
    QCOMPARE(m.value(QStringLiteral("firstWeekday")).toInt(), first.dayOfWeek() - 1);
    QCOMPARE(dayEntry(m, 5).value(QStringLiteral("secs")).toLongLong(), 2400LL);
    QCOMPARE(booksOn(m, 5).size(), 2);
    /* Longest first: the day panel reads top-down. */
    QCOMPARE(booksOn(m, 5).at(0).toMap().value(QStringLiteral("title")).toString(),
             QStringLiteral("Buch A"));
    QCOMPARE(booksOn(m, 5).at(1).toMap().value(QStringLiteral("author")).toString(),
             QStringLiteral("Autor"));
    /* Under a minute is not on the calendar at all: not as a book, not as
     * seconds on the day, and not in the month's total. */
    QVERIFY(booksOn(m, 6).isEmpty());
    QCOMPARE(dayEntry(m, 6).value(QStringLiteral("secs")).toLongLong(), 0LL);
    QCOMPARE(m.value(QStringLiteral("readDays")).toInt(), 1);
    QCOMPARE(m.value(QStringLiteral("totalSecs")).toLongLong(), 2400LL);
}

/* books_impl keeps stale rows and the same book exists as several file copies,
 * so the same reading arrives under two book_ids and two spellings. */
void TestStatsBridge::monthMergesTheSameBookUnderItsVariantTitles()
{
    insertOwnBook(device_->statsDb(), 1, QStringLiteral("Der Steppenwolf"),
                  QStringLiteral("Hesse"), QString());
    insertOwnBook(device_->statsDb(), 2, QStringLiteral("der steppenwolf"),
                  QStringLiteral("Hesse"), QString());
    insertSession(device_->statsDb(),
                  {1, noonOn(dayOfThisMonth(7)), noonOn(dayOfThisMonth(7)) + 1800, 1800, 30, 0});
    insertSession(device_->statsDb(),
                  {2, noonOn(dayOfThisMonth(7)) + 3600, noonOn(dayOfThisMonth(7)) + 5400,
                   1800, 30, 0});

    StatsBridge stats;
    const QVariantMap m = stats.month(QDate::currentDate().year(), QDate::currentDate().month());

    QCOMPARE(booksOn(m, 7).size(), 1);
    QCOMPARE(booksOn(m, 7).at(0).toMap().value(QStringLiteral("secs")).toLongLong(), 3600LL);
}

/* The firmware dates a finish without saying how long that day's reading was,
 * so a finish is drawn on the calendar even where nothing was measured — the
 * one exception to "nothing before tracking_since is history". */
void TestStatsBridge::monthCarriesFirmwareFinishesEvenBeforeTracking()
{
    const QDate day = dayOfThisMonth(9);
    BookRow b;
    b.id = 3;
    b.title = QStringLiteral("Ausgelesen");
    b.author = QStringLiteral("Autorin");
    b.completed = 1;
    b.completedTs = noonOn(day);
    insertBook(device_->explorerDb(), b);
    /* A stale duplicate of the same book: books_impl keeps those, and the
     * calendar must not draw the finish twice. */
    BookRow stale = b;
    stale.id = 33;
    stale.title = QStringLiteral("Ausgelesen : Roman");
    insertBook(device_->explorerDb(), stale);

    StatsBridge stats;
    const QVariantMap m = stats.month(day.year(), day.month());

    QCOMPARE(booksOn(m, 9).size(), 1);
    const QVariantMap entry = booksOn(m, 9).at(0).toMap();
    QVERIFY(entry.value(QStringLiteral("finished")).toBool());
    QCOMPARE(entry.value(QStringLiteral("secs")).toLongLong(), 0LL);
    /* Carrying no reading time, it is not a day that was read. */
    QCOMPARE(m.value(QStringLiteral("readDays")).toInt(), 0);

    /* Where the day was measured too, the finish is a mark on that entry
     * rather than a second book. */
    insertOwnBook(device_->statsDb(), 3, QStringLiteral("Ausgelesen"),
                  QStringLiteral("Autorin"), QString());
    insertSession(device_->statsDb(), {3, noonOn(day), noonOn(day) + 1800, 1800, 30, 0});
    const QVariantList books = booksOn(stats.month(day.year(), day.month()), 9);
    QCOMPARE(books.size(), 1);
    QVERIFY(books.at(0).toMap().value(QStringLiteral("finished")).toBool());
    QCOMPARE(books.at(0).toMap().value(QStringLiteral("secs")).toLongLong(), 1800LL);
}

/* A screen that can show a blank day has to say why it is blank. */
void TestStatsBridge::monthMarksWhereTrackingBegins()
{
    const QDate since = dayOfThisMonth(10);
    setMeta(device_->statsDb(), QStringLiteral("tracking_since"), noonOn(since));

    StatsBridge stats;
    const QVariantMap m = stats.month(since.year(), since.month());
    QCOMPARE(m.value(QStringLiteral("trackedFromDay")).toInt(), 10);
    QCOMPARE(m.value(QStringLiteral("trackingSince")).toString(),
             since.toString(QStringLiteral("dd.MM.yyyy")));

    /* A month entirely before the marker is unknown from end to end... */
    const QDate earlier = since.addMonths(-1);
    const QVariantMap old = stats.month(earlier.year(), earlier.month());
    QCOMPARE(old.value(QStringLiteral("trackedFromDay")).toInt(),
             earlier.daysInMonth() + 1);
    /* ...and one entirely after it was measured from its first day. */
    const QDate later = since.addMonths(1);
    QCOMPARE(stats.month(later.year(), later.month())
                 .value(QStringLiteral("trackedFromDay")).toInt(), 0);
}

/* ---- the streak screen -------------------------------------------------- */

void TestStatsBridge::yearFeedsTheStreakScreen()
{
    const QDate today = QDate::currentDate();
    const QDate first(today.year(), 1, 1);
    /* Two in a row up to today, and a longer run earlier in the year. */
    for (int back = 0; back <= 1; back++)
        insertSession(device_->statsDb(),
                      {1, noonOn(today.addDays(-back)),
                       noonOn(today.addDays(-back)) + 1800, 1800, 0, 0});
    /* A longer run, kept clear of the two days above whatever day of the year
     * the suite runs on. The grid holds the whole year, so a run in the days
     * ahead is as good a fixture as one behind. */
    const QDate runStart = today.dayOfYear() > 20 ? today.addDays(-10)
                                                  : today.addDays(10);
    for (int i = 0; i < 4; i++)
        insertSession(device_->statsDb(),
                      {1, noonOn(runStart.addDays(i)), noonOn(runStart.addDays(i)) + 1800,
                       1800, 0, 0});

    StatsBridge stats;
    const QVariantMap m = stats.year(today.year());

    QCOMPARE(m.value(QStringLiteral("year")).toInt(), today.year());
    QCOMPARE(m.value(QStringLiteral("ndays")).toInt(), first.daysInYear());
    QCOMPARE(m.value(QStringLiteral("firstWeekday")).toInt(), first.dayOfWeek() - 1);
    QCOMPARE(m.value(QStringLiteral("readDays")).toInt(), 6);
    QCOMPARE(m.value(QStringLiteral("current")).toInt(), 2);
    QCOMPARE(m.value(QStringLiteral("best")).toInt(), 4);

    const QVariantList days = m.value(QStringLiteral("days")).toList();
    QCOMPARE(days.size(), first.daysInYear());
    QCOMPARE(days.at(int(first.daysTo(today))).toInt(), 1);
    QCOMPARE(days.at(int(first.daysTo(runStart))).toInt(), 1);
    QCOMPARE(days.at(int(first.daysTo(runStart)) + 4).toInt(), 0);

    /* Days before the marker are neither read nor unread. */
    setMeta(device_->statsDb(), QStringLiteral("tracking_since"), noonOn(runStart));
    QCOMPARE(stats.year(today.year()).value(QStringLiteral("trackedFrom")).toInt(),
             int(first.daysTo(runStart)));
}

/* ---- covers ------------------------------------------------------------- */

/* Most finished books get deleted, and the firmware drops its cache entry with
 * the file. Our own copy is the only thing that survives that, so it wins. */
void TestStatsBridge::coverComesFromOurCacheFirst()
{
    const QString hash = QStringLiteral("aabbccdd");
    BookRow b;
    b.id = 4;
    b.title = QStringLiteral("Mit Bild");
    b.hash = hash;
    b.opentime = noonOn(QDate::currentDate());
    b.positionTs = b.opentime + 60;
    b.npage = 10;
    insertBook(device_->explorerDb(), b);

    device_->write(QStringLiteral(OWN_COVER_DIR "/") + hash + QStringLiteral(".png"),
                   pngBytes(60, 100, kOurs));
    /* The firmware has one too, and it must not be the one that is used. */
    device_->write(QStringLiteral(COVER_DIR "/1") + hash + QStringLiteral(".png"),
                   pngBytes(60, 100, kFirmware));

    StatsBridge stats;
    const QString url = stats.currentBook().value(QStringLiteral("coverUrl")).toString();
    QCOMPARE(url, QUrl::fromLocalFile(devicePath(OWN_COVER_DIR "/") + hash
                                      + QStringLiteral(".png")).toString());
}

/* No file and no cache of ours: the firmware's copy is used and adopted on the
 * way out, under a key of its own so a later extraction can still win. */
void TestStatsBridge::coverIsAdoptedFromTheFirmwareCache()
{
    const QString hash = QStringLiteral("beef1234");
    BookRow b;
    b.id = 4;
    b.title = QStringLiteral("Nur Firmware");
    b.hash = hash;
    b.opentime = noonOn(QDate::currentDate());
    b.positionTs = b.opentime + 60;
    b.npage = 10;
    insertBook(device_->explorerDb(), b);
    device_->write(QStringLiteral(COVER_DIR "/1") + hash + QStringLiteral(".png"),
                   pngBytes(60, 100, kFirmware));

    StatsBridge stats;
    const QString url = stats.currentBook().value(QStringLiteral("coverUrl")).toString();

    const QString adopted = devicePath(OWN_COVER_DIR "/fw_") + hash + QStringLiteral(".png");
    QCOMPARE(url, QUrl::fromLocalFile(adopted).toString());
    QVERIFY2(QFile::exists(adopted), "the firmware's copy outlives its cache entry");
}

/* Rows written before v0.7 hold <storageid><hash> — 33 characters instead of
 * 32, which is how the key tells the two apart. */
void TestStatsBridge::aLegacyCoverKeyStillResolves()
{
    const QString hash = QStringLiteral("0123456789abcdef0123456789abcdef");
    device_->write(QStringLiteral(OWN_COVER_DIR "/") + hash + QStringLiteral(".png"),
                   pngBytes(60, 100, kOurs));
    insertOwnBook(device_->statsDb(), 1, QStringLiteral("Altes Buch"),
                  QStringLiteral("Autorin"), QStringLiteral("1") + hash);
    insertSession(device_->statsDb(),
                  {1, noonOn(dayOfThisMonth(11)), noonOn(dayOfThisMonth(11)) + 1800,
                   1800, 0, 0});

    StatsBridge stats;
    const QVariantList books =
        booksOn(stats.month(QDate::currentDate().year(), QDate::currentDate().month()), 11);

    QCOMPARE(books.size(), 1);
    QCOMPARE(books.at(0).toMap().value(QStringLiteral("coverUrl")).toString(),
             QUrl::fromLocalFile(devicePath(OWN_COVER_DIR "/") + hash
                                 + QStringLiteral(".png")).toString());
}

/* The firmware's cache holds an ad page instead of the cover for some Calibre
 * books, so an extraction from the file itself always wins over it. Our own
 * cache still wins over both — it is the only one that outlives the file. */
void TestStatsBridge::theBookFileBeatsTheFirmwareCache()
{
    const QString hash = QStringLiteral("cafe0001");
    device_->write(QStringLiteral("/books/real.epub"), epub2(pngBytes(120, 200, kOurs)));
    device_->write(QStringLiteral(COVER_DIR "/1") + hash + QStringLiteral(".png"),
                   pngBytes(60, 100, kFirmware));

    BookRow b;
    b.id = 4;
    b.title = QStringLiteral("Mit Datei");
    b.hash = hash;
    b.opentime = noonOn(QDate::currentDate());
    b.positionTs = b.opentime + 60;
    b.npage = 10;
    b.folder = device_->at(QStringLiteral("/books"));
    b.filename = QStringLiteral("real.epub");
    insertBook(device_->explorerDb(), b);

    StatsBridge stats;
    const QString url = stats.currentBook().value(QStringLiteral("coverUrl")).toString();

    const QString extracted = devicePath(OWN_COVER_DIR "/") + hash + QStringLiteral(".png");
    QCOMPARE(url, QUrl::fromLocalFile(extracted).toString());
    const QImage img(extracted);
    QVERIFY(!img.isNull());
    QCOMPARE(img.pixelColor(img.width() / 2, img.height() / 2).name(),
             QColor(QLatin1String(kOurs)).name());
}

/* books_impl keeps a row per copy, and the cover lookup matches on the exact
 * title — so of two spellings of one book, the merge has to keep the one that
 * still has a file behind it. */
void TestStatsBridge::theVariantThatStillHasAFileWins()
{
    const QDate day = dayOfThisMonth(12);
    device_->write(QStringLiteral("/books/roman.epub"), QByteArrayLiteral("x"));

    BookRow gone;
    gone.id = 5;
    gone.title = QStringLiteral("Buch");
    gone.completed = 1;
    gone.completedTs = noonOn(day);
    insertBook(device_->explorerDb(), gone);

    BookRow kept;
    kept.id = 6;
    kept.title = QStringLiteral("Buch : Roman");
    kept.author = QStringLiteral("Autorin");
    kept.completed = 1;
    kept.completedTs = noonOn(day) + 60;
    kept.folder = device_->at(QStringLiteral("/books"));
    kept.filename = QStringLiteral("roman.epub");
    insertBook(device_->explorerDb(), kept);

    StatsBridge stats;
    const QVariantList books = booksOn(stats.month(day.year(), day.month()), 12);

    QCOMPARE(books.size(), 1);
    QCOMPARE(books.at(0).toMap().value(QStringLiteral("title")).toString(), kept.title);
    QCOMPARE(books.at(0).toMap().value(QStringLiteral("author")).toString(), kept.author);
    /* One book, counted once, whichever spelling it came under. */
    QCOMPARE(stats.overall().value(QStringLiteral("booksFinished")).toInt(), 1);
}

QTEST_GUILESS_MAIN(TestStatsBridge)
#include "tst_stats_bridge.moc"
