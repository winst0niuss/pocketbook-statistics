#include "stats_bridge.h"

#include <QDate>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QRegularExpression>
#include <QUrl>
#include <QVariantList>
#include <QVector>

#include "book_cover.h"
#include "inkview_bridge.h"
#include "update_log.h"

extern "C" {
#include "daemon.h"
#include "stats_db.h"
#include "tracker.h"
}


StatsBridge::StatsBridge(QObject *parent) : QObject(parent)
{
    if (sqlite3_open(stats_db_path(), &db_) == SQLITE_OK) {
        sqlite3_busy_timeout(db_, 2000);
    } else {
        /* Every figure on both screens comes from this file. Without it the
         * app still opens and shows zeros, which looks like "you have not read
         * anything" rather than a fault. */
        updateLog(QStringLiteral("app: cannot open the stats DB at %1")
                      .arg(QString::fromLatin1(stats_db_path())));
        /* sqlite3_open returns a handle even on failure — it is what carries
         * the error message — and it has to be closed, not dropped. */
        sqlite3_close(db_);
        db_ = nullptr;
    }
}

StatsBridge::~StatsBridge()
{
    if (db_)
        sqlite3_close(db_);
}

/* The daemon polls every 30 s, so closing a book and opening this app lands in
 * the middle of that window: the screen then shows the state from up to half a
 * minute ago, which is exactly the reading just done. And if the daemon was
 * killed while the reader held the foreground, nothing wrote that session at
 * all until the next launch spawned it again — which is why restarting the app
 * "fixed" it.
 *
 * So take the reading ourselves before every aggregate. It costs one read-only
 * query against explorer-3 and at most one INSERT OR IGNORE plus one UPDATE:
 * this is the same tracker the daemon runs, sessions are keyed on
 * (book_id, start_time), and a row can only gain seconds the firmware has
 * already recorded. Both processes writing at once is fine — tracker_init sets
 * a busy timeout — and they cannot double-count the same gap, because
 * position_ts does not advance while the reader is closed. */
void StatsBridge::catchUp()
{
    tracker t;
    if (tracker_init(&t, stats_db_path(), explorer_db_path()) != 0) {
        /* Once only: this runs on every refresh, and the screen refreshes once
         * a minute — a failure here would otherwise fill a week's log with the
         * same line and push everything else out of it. */
        static bool logged = false;
        if (!logged) {
            logged = true;
            updateLog(QStringLiteral("app: catchUp could not open the databases"));
        }
        return;
    }
    pb_state s;
    if (tracker_read_state(t.explorer_path, &s) == 0)
        tracker_observe(&t, &s);
    tracker_close(&t);
}

namespace {

/* Cover for the key stored in our own books table. Rows written before v0.7
 * hold <storageid><hash>; newer ones hold the bare hash, because the storage
 * id came from files and vanished with the file. */
QString coverUrlForKey(sqlite3 *expdb, const QString &key);

sqlite3 *openExplorer()
{
    sqlite3 *db = nullptr;
    if (sqlite3_open_v2(explorer_db_path(), &db, SQLITE_OPEN_READONLY,
                        nullptr) != SQLITE_OK) {
        if (db)
            sqlite3_close(db);
        return nullptr;
    }
    sqlite3_busy_timeout(db, 1000);
    return db;
}

/* The firmware names its cover files <storageid><hash>.png, and the same book
 * can be indexed on several storages (internal, cloud, SD). */
QList<int> storageIds(sqlite3 *expdb)
{
    QList<int> ids;
    sqlite3_stmt *st = nullptr;
    if (sqlite3_prepare_v2(expdb, "SELECT id FROM storages ORDER BY id", -1,
                           &st, nullptr) == SQLITE_OK) {
        while (sqlite3_step(st) == SQLITE_ROW)
            ids.append(sqlite3_column_int(st, 0));
    }
    sqlite3_finalize(st);
    if (ids.isEmpty())
        ids << 1; /* internal storage, the only one that always exists */
    return ids;
}

/* Copies a firmware cover into our own cache under "fw_<hash>". Kept apart
 * from the EPUB extraction's key so a book whose file is still around can
 * later be upgraded to the better image, and kept at all because the firmware
 * drops its cache entry when the book is deleted — this copy is what a
 * finished-and-deleted book has left. */
QString adoptedCover(const QString &hash, const QList<int> &storages)
{
    if (hash.isEmpty())
        return QString();
    const QString mine =
        QStringLiteral(OWN_COVER_DIR "/fw_%1.png").arg(hash);
    if (QFile::exists(mine))
        return QUrl::fromLocalFile(mine).toString();

    for (int storage : storages) {
        const QString source =
            QStringLiteral(COVER_DIR "/%1%2.png").arg(storage).arg(hash);
        if (!QFile::exists(source))
            continue;
        QDir().mkpath(QLatin1String(OWN_COVER_DIR));
        if (QFile::copy(source, mine))
            return QUrl::fromLocalFile(mine).toString();
        return QUrl::fromLocalFile(source).toString(); /* copy failed, show it anyway */
    }
    return QString();
}

QString coverUrlForKey(sqlite3 *expdb, const QString &key)
{
    if (key.isEmpty())
        return QString();
    const QString hash = key.size() == 33 ? key.mid(1) : key;
    const QString cached = bookCover(QString(), hash);
    if (!cached.isEmpty())
        return QUrl::fromLocalFile(cached).toString();
    return expdb ? adoptedCover(hash, storageIds(expdb)) : QString();
}

/* Where a book's file is, or nothing. `files` is an inventory of what exists
 * right now, so most books have no row at all — and the rows that are there
 * are not all local: a cloud copy is filed under a storage whose folder has no
 * name, which would resolve to "/<filename>" at the root. Hence both the test
 * on the folder and the one on disk; local storages sort first, and the first
 * path that is actually there wins. */
QString bookFilePath(sqlite3 *expdb, qint64 bookId)
{
    if (!expdb)
        return QString();
    const char *sql =
        "SELECT fo.name || '/' || f.filename"
        " FROM files f JOIN folders fo ON fo.id = f.folder_id"
        " WHERE f.book_id = ?1 AND IFNULL(fo.name,'') <> ''"
        " ORDER BY f.storageid ASC";
    sqlite3_stmt *st = nullptr;
    if (sqlite3_prepare_v2(expdb, sql, -1, &st, nullptr) != SQLITE_OK)
        return QString();
    sqlite3_bind_int64(st, 1, bookId);
    QString found;
    while (found.isEmpty() && sqlite3_step(st) == SQLITE_ROW) {
        const QString path = QString::fromUtf8(
            reinterpret_cast<const char *>(sqlite3_column_text(st, 0)));
        if (!path.isEmpty() && QFileInfo::exists(path))
            found = path;
    }
    sqlite3_finalize(st);
    return found;
}

/* Cover for a title, in the order that survives the book being deleted:
 *   1. our own cache, written by any earlier lookup — outlives everything;
 *   2. the book file itself — EPUB, FB2, CBZ — the best image, but only while
 *      the file is there (the firmware cache holds ad pages instead of the
 *      cover for some Calibre books, so it never wins over an extraction);
 *   3. the firmware's cache, copied into ours on the way out.
 *
 * The hash comes from books_fast_hashes, not from files: files only lists what
 * is physically present, which on a well-used reader is a handful of books out
 * of hundreds, and every deleted book would fall through to a bare letter. */
QString resolveCoverUrl(sqlite3 *expdb, const QString &title)
{
    if (!expdb || title.isEmpty())
        return QString();
    const char *sql =
        "SELECT lower(hex(h.fast_hash)),"
        " IFNULL(fo.name,'') || '/' || IFNULL(f.filename,'')"
        " FROM books_impl b"
        " JOIN books_fast_hashes h ON h.book_id = b.id"
        " LEFT JOIN files f ON f.book_id = b.id"
        " LEFT JOIN folders fo ON fo.id = f.folder_id"
        " WHERE lower(trim(b.title)) = lower(trim(?1))"
        " ORDER BY (f.filename IS NOT NULL) DESC, f.modification_time DESC,"
        "          f.storageid ASC";
    sqlite3_stmt *st = nullptr;
    if (sqlite3_prepare_v2(expdb, sql, -1, &st, nullptr) != SQLITE_OK)
        return QString();
    sqlite3_bind_text(st, 1, title.toUtf8().constData(), -1, SQLITE_TRANSIENT);

    struct Candidate { QString hash, path; };
    QList<Candidate> candidates;
    while (sqlite3_step(st) == SQLITE_ROW) {
        Candidate c;
        c.hash = QString::fromUtf8(
            reinterpret_cast<const char *>(sqlite3_column_text(st, 0)));
        c.path = QString::fromUtf8(
            reinterpret_cast<const char *>(sqlite3_column_text(st, 1)));
        if (c.path.endsWith(QLatin1Char('/')))
            c.path.clear(); /* no files row: the join left an empty filename */
        candidates.append(c);
    }
    sqlite3_finalize(st);

    /* 1. anything we cached earlier, extraction or adoption */
    for (const Candidate &c : candidates) {
        const QString cached = bookCover(QString(), c.hash);
        if (!cached.isEmpty())
            return QUrl::fromLocalFile(cached).toString();
    }
    /* 2. the file itself */
    for (const Candidate &c : candidates) {
        if (c.path.isEmpty())
            continue;
        const QString extracted = bookCover(c.path, c.hash);
        if (!extracted.isEmpty())
            return QUrl::fromLocalFile(extracted).toString();
    }
    /* 3. the firmware cache, adopted into ours */
    const QList<int> storages = storageIds(expdb);
    for (const Candidate &c : candidates) {
        const QString adopted = adoptedCover(c.hash, storages);
        if (!adopted.isEmpty())
            return adopted;
    }
    return QString();
}

struct FinishedBook {
    QDate day;
    QString title;
    QString author;
    QString coverUrl;
    /* The firmware's page count, as the Library shows it: repaginated for the
     * current font, and for some reflowable books a percentage (100) rather
     * than pages. Callers that add these up must drop the 100s. */
    int pages = 0;
    QStringList words; /* normalized title, for the dedupe below */
    bool hasFile = false;
};

/* Title as lowercase words, punctuation dropped. */
QStringList titleWords(const QString &title)
{
    static const QRegularExpression sep(QStringLiteral("[^\\p{L}\\p{N}]+"));
    return title.toLower().split(sep, Qt::SkipEmptyParts);
}

/* Same book if one word list is a prefix of the other. Catches the variants
 * books_impl accumulates ("… : Roman", punctuation) without merging series
 * volumes or "Die Nacht" vs "Die Nachtigall". */
bool sameBook(const QStringList &a, const QStringList &b)
{
    const int n = qMin(a.size(), b.size());
    if (n == 0)
        return false;
    for (int i = 0; i < n; i++)
        if (a[i] != b[i])
            return false;
    return true;
}

/* Finished books straight from the firmware DB: completed_ts is set by the
 * firmware exactly on "mark as read" (DB trigger). Cover extraction is the
 * expensive part, so callers that only need dates/counts skip it. */
QList<FinishedBook> finishedBooks(bool withCovers)
{
    QList<FinishedBook> out;
    sqlite3 *db = openExplorer();
    if (!db)
        return out;
    /* One row per entry, oldest finish first; books_impl keeps stale rows
     * after delete/re-add, so the merge below folds them together. */
    const char *sql =
        "SELECT date(s.completed_ts,'unixepoch','localtime'), IFNULL(b.title,'?'),"
        " EXISTS(SELECT 1 FROM files f WHERE f.book_id = b.id),"
        " IFNULL(b.author,''), IFNULL(s.npage,0)"
        " FROM books_settings s JOIN books_impl b ON b.id = s.bookid"
        " WHERE s.completed = 1 AND s.completed_ts > 0"
        " ORDER BY s.completed_ts";
    sqlite3_stmt *st = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &st, nullptr) == SQLITE_OK) {
        while (sqlite3_step(st) == SQLITE_ROW) {
            const QDate day = QDate::fromString(
                QString::fromUtf8(reinterpret_cast<const char *>(
                    sqlite3_column_text(st, 0))),
                Qt::ISODate);
            if (!day.isValid())
                continue;
            const QString title = QString::fromUtf8(
                reinterpret_cast<const char *>(sqlite3_column_text(st, 1)));
            const bool hasFile = sqlite3_column_int(st, 2) != 0;
            const QString author = QString::fromUtf8(
                reinterpret_cast<const char *>(sqlite3_column_text(st, 3)));
            const int pages = sqlite3_column_int(st, 4);
            const QStringList words = titleWords(title);

            int at = -1;
            for (int i = 0; i < out.size() && at < 0; i++)
                if (sameBook(out[i].words, words))
                    at = i;
            if (at < 0) {
                FinishedBook fb;
                fb.day = day; /* first row of a group = first finish date */
                fb.title = title;
                fb.author = author;
                fb.pages = pages;
                fb.words = words;
                fb.hasFile = hasFile;
                out.append(fb);
            } else if (hasFile && !out[at].hasFile) {
                /* the variant that still has a file wins: the cover lookup
                 * matches on the exact title */
                out[at].title = title;
                out[at].words = words;
                out[at].hasFile = true;
                if (!author.isEmpty())
                    out[at].author = author;
                if (pages > out[at].pages)
                    out[at].pages = pages;
            }
        }
    }
    sqlite3_finalize(st);
    if (withCovers) {
        for (FinishedBook &fb : out)
            fb.coverUrl = resolveCoverUrl(db, fb.title);
    }
    sqlite3_close(db);
    return out;
}

} // namespace

QVariantMap StatsBridge::overall()
{
    catchUp();
    overall_stats o;
    stats_overall(db_, &o);
    QVariantMap m;
    /* Per hour, not per minute: a real reading speed is 0.1 pages a minute,
     * which reads as a broken counter. The same number as 6 pages an hour
     * reads as a fact. */
    m[QStringLiteral("pagesPerHour")] = o.pages_per_min * 60.0;
    /* The Overview shows what was measured, and only that: minutes today, the
     * pace behind them, books finished ever and hours in total. The ring that
     * used to sit here answered a different question — how much of the shelf
     * currently on the device has been read — and with it goes the query that
     * fed it, which was the one place this call touched the firmware library. */
    m[QStringLiteral("todaySecs")] = o.today_secs;
    /* The two all-time figures, and the only two the hand-set offsets touch.
     * Today's minutes, the pace and everything the other screens show stay as
     * they were measured. */
    m[QStringLiteral("totalHours")] =
        o.total_hours + stats_meta_int(db_, META_OFFSET_SECONDS) / 3600.0;
    m[QStringLiteral("booksFinished")] =
        finishedBooks(false).size() + int(stats_meta_int(db_, META_OFFSET_BOOKS));
    /* Days in a row, for the line the About screen opens with. */
    m[QStringLiteral("streakDays")] = o.streak_days;

    return m;
}

QVariantMap StatsBridge::currentBook()
{
    catchUp();
    QVariantMap m;
    pb_state s;
    if (tracker_read_state(explorer_db_path(), &s) != 0) {
        m[QStringLiteral("ok")] = false;
        return m;
    }
    m[QStringLiteral("ok")] = true;
    m[QStringLiteral("title")] = QString::fromUtf8(s.title);
    m[QStringLiteral("author")] = QString::fromUtf8(s.author);
    double prog = s.completed ? 1.0
                              : (s.npage > 0 ? double(s.cpage) / s.npage : 0.0);
    m[QStringLiteral("percent")] = int(prog * 100 + 0.5);

    QString coverUrl;
    /* Empty unless the book is still on the device: the card is only tappable
     * where there is something left to open. */
    QString filePath;
    if (sqlite3 *exp = openExplorer()) {
        coverUrl = resolveCoverUrl(exp, QString::fromUtf8(s.title));
        filePath = bookFilePath(exp, s.bookid);
        sqlite3_close(exp);
    }
    m[QStringLiteral("filePath")] = filePath;
    if (coverUrl.isEmpty()) {
        if (sqlite3 *exp = openExplorer()) {
            coverUrl = coverUrlForKey(exp, QString::fromUtf8(s.cover));
            sqlite3_close(exp);
        }
    }
    m[QStringLiteral("coverUrl")] = coverUrl;

    /* Only the pace is wanted: it turns pages left into time left. The seconds
     * this book has taken were shown until the Overview was rebuilt around
     * today's minutes instead. */
    int64_t bsecs = 0;
    double bppm = 0;
    stats_book(db_, s.bookid, &bsecs, &bppm);
    if (bppm <= 0) {
        overall_stats o;
        stats_overall(db_, &o);
        bppm = o.pages_per_min;
    }
    qlonglong left = 0;
    if (bppm > 0 && s.npage > s.cpage && !s.completed)
        left = qlonglong((s.npage - s.cpage) / bppm * 60.0);
    m[QStringLiteral("leftSecs")] = left;
    return m;
}


QVariantMap StatsBridge::month(int year, int mon)
{
    catchUp();
    QVariantMap m;
    const QDate first(year, mon, 1);
    const int ndays = first.daysInMonth();
    m[QStringLiteral("ndays")] = ndays;
    m[QStringLiteral("firstWeekday")] = first.dayOfWeek() - 1; /* 0 = Montag */

    /* Per day: books with reading time, sorted descending. */
    QVector<QVariantList> perDay(ndays + 1);
    QVector<qlonglong> daySecs(ndays + 1, 0);
    /* GROUP BY title instead of book_id: the same books exist as several file
     * copies and would otherwise appear twice. */
    const char *sql =
        "SELECT CAST(strftime('%d', s.end_time,'unixepoch','localtime') AS INTEGER),"
        " MIN(IFNULL(b.title,'?')), MAX(IFNULL(b.cover,'')), SUM(s.active_seconds),"
        " MAX(IFNULL(b.author,''))"
        " FROM sessions s LEFT JOIN books b ON b.book_id = s.book_id"
        " WHERE strftime('%Y-%m', s.end_time,'unixepoch','localtime')"
        "   = printf('%04d-%02d',?1,?2)"
        " AND s.end_time >= " TRACKED_SINCE_SQL
        " GROUP BY 1, lower(trim(IFNULL(b.title,'?')))"
        " HAVING SUM(s.active_seconds) >= 60"
        " ORDER BY 1, 4 DESC";
    sqlite3 *exp = openExplorer();
    QHash<QString, QString> coverCache; /* title -> URL, once per call */
    sqlite3_stmt *st = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &st, nullptr) == SQLITE_OK) {
        sqlite3_bind_int(st, 1, year);
        sqlite3_bind_int(st, 2, mon);
        while (sqlite3_step(st) == SQLITE_ROW) {
            int d = sqlite3_column_int(st, 0);
            if (d < 1 || d > ndays)
                continue;
            QVariantMap b;
            const QString title = QString::fromUtf8(
                reinterpret_cast<const char *>(sqlite3_column_text(st, 1)));
            b[QStringLiteral("title")] = title;
            const QString cover = QString::fromUtf8(
                reinterpret_cast<const char *>(sqlite3_column_text(st, 2)));
            if (!coverCache.contains(title)) {
                QString url = resolveCoverUrl(exp, title);
                if (url.isEmpty())
                    url = coverUrlForKey(exp, cover);
                coverCache.insert(title, url);
            }
            b[QStringLiteral("coverUrl")] = coverCache.value(title);
            b[QStringLiteral("author")] = QString::fromUtf8(
                reinterpret_cast<const char *>(sqlite3_column_text(st, 4)));
            qlonglong secs = sqlite3_column_int64(st, 3);
            b[QStringLiteral("secs")] = secs;
            perDay[d].append(b);
            daySecs[d] += secs;
        }
    }
    sqlite3_finalize(st);
    if (exp)
        sqlite3_close(exp);

    /* Finish dates come from the firmware, so they belong on the calendar even
     * for days before tracking started — the one exception to the rule that
     * nothing before meta.tracking_since is history. They carry no reading
     * time: the firmware dates the finish, it does not say how long the day's
     * reading was. */
    const QList<FinishedBook> finished = finishedBooks(true);
    for (const FinishedBook &fb : finished) {
        if (fb.day.year() != year || fb.day.month() != mon)
            continue;
        const int d = fb.day.day();
        if (d < 1 || d > ndays)
            continue;

        bool merged = false;
        for (QVariant &entry : perDay[d]) {
            QVariantMap b = entry.toMap();
            if (!sameBook(titleWords(b.value(QStringLiteral("title")).toString()),
                          fb.words))
                continue;
            b[QStringLiteral("finished")] = true;
            entry = b;
            merged = true;
            break;
        }
        if (merged)
            continue;

        QVariantMap b;
        b[QStringLiteral("title")] = fb.title;
        b[QStringLiteral("author")] = fb.author;
        b[QStringLiteral("coverUrl")] = fb.coverUrl;
        b[QStringLiteral("secs")] = 0;
        b[QStringLiteral("finished")] = true;
        perDay[d].append(b);
    }

    /* Days before the app was installed hold no measurement. The tab draws
     * them as unknown rather than as days without reading. */
    const qint64 since = stats_tracking_since(db_);
    const QDate sinceDate = since > 0
        ? QDateTime::fromSecsSinceEpoch(since).date() : QDate();
    int trackedFromDay = 0;
    if (sinceDate.isValid()) {
        if (sinceDate > first.addDays(ndays - 1))
            trackedFromDay = ndays + 1; /* the whole month predates tracking */
        else if (sinceDate > first)
            trackedFromDay = sinceDate.day();
    }
    m[QStringLiteral("trackedFromDay")] = trackedFromDay;
    m[QStringLiteral("trackingSince")] = sinceDate.isValid()
        ? sinceDate.toString(QStringLiteral("dd.MM.yyyy")) : QString();

    QVariantList days;
    qlonglong totalSecs = 0;
    int readDays = 0;
    for (int d = 1; d <= ndays; d++) {
        QVariantMap e;
        e[QStringLiteral("secs")] = daySecs[d];
        e[QStringLiteral("books")] = perDay[d];
        days.append(e);
        totalSecs += daySecs[d];
        if (daySecs[d] > 0)
            readDays++;
    }
    m[QStringLiteral("totalSecs")] = totalSecs;
    m[QStringLiteral("readDays")] = readDays;
    m[QStringLiteral("days")] = days;
    return m;
}

/* The year behind the streak: one entry per day for the grid, the two figures
 * over it, and the marker that says where measurement begins.
 *
 * A day is read or it is not: the grid drew the firmware's "finished" marks
 * too until it turned out that a third symbol says nothing the two others do
 * not, and a finish carries no reading time to place it by anyway. Runs are
 * clipped at the turn of the year, which is what "best in 2026" means.
 *
 * Days before tracking_since are neither read nor unread — nothing was
 * measuring — so they are marked out separately and the screen draws them as
 * unknown, the way the calendar does. */
QVariantMap StatsBridge::year(int y)
{
    catchUp();
    QVariantMap m;
    const QDate first(y, 1, 1);
    const int ndays = first.daysInYear();
    m[QStringLiteral("year")] = y;
    m[QStringLiteral("ndays")] = ndays;
    /* 0 = Monday, so the grid's rows line up with the calendar's. */
    m[QStringLiteral("firstWeekday")] = first.dayOfWeek() - 1;

    QVector<unsigned char> days(ndays, 0);
    const int readDays = stats_year_days(db_, y, days.data(), ndays);
    m[QStringLiteral("readDays")] = readDays;
    m[QStringLiteral("current")] = stats_streak_days(db_);

    int best = 0, run = 0;
    for (int i = 0; i < ndays; i++) {
        run = days[i] ? run + 1 : 0;
        if (run > best)
            best = run;
    }
    m[QStringLiteral("best")] = best;

    QVariantList list;
    list.reserve(ndays);
    for (int i = 0; i < ndays; i++)
        list.append(int(days[i]));
    m[QStringLiteral("days")] = list;

    const qint64 since = stats_tracking_since(db_);
    const QDate sinceDate = since > 0
        ? QDateTime::fromSecsSinceEpoch(since).date() : QDate();
    int trackedFrom = 0; /* 0 = the whole year was measured */
    if (sinceDate.isValid()) {
        if (sinceDate > first.addDays(ndays - 1))
            trackedFrom = ndays; /* the whole year predates tracking */
        else if (sinceDate > first)
            trackedFrom = int(first.daysTo(sinceDate));
    }
    m[QStringLiteral("trackedFrom")] = trackedFrom;
    m[QStringLiteral("trackingSince")] = sinceDate.isValid()
        ? sinceDate.toString(QStringLiteral("dd.MM.yyyy")) : QString();
    return m;
}

/* An offset, never a measurement: what is stored is the difference between the
 * figure the card should show and the one that was measured, so a session
 * recorded tomorrow still adds to the total. Clearing is a negative argument
 * rather than a call of its own — the dialog's reset button is the same button
 * as its steps, one call away either way.
 *
 * Rounded to the second, because that is what the column holds and a total of
 * "282.4 hours" is not a figure anybody typed. */
void StatsBridge::setTotalHours(double hours)
{
    if (hours < 0) {
        stats_meta_set_int(db_, META_OFFSET_SECONDS, 0);
        return;
    }
    overall_stats o;
    stats_overall(db_, &o);
    stats_meta_set_int(db_, META_OFFSET_SECONDS,
                       qRound64((hours - o.total_hours) * 3600.0));
}

void StatsBridge::setBooksFinished(int books)
{
    if (books < 0) {
        stats_meta_set_int(db_, META_OFFSET_BOOKS, 0);
        return;
    }
    stats_meta_set_int(db_, META_OFFSET_BOOKS, books - finishedBooks(false).size());
}

/* The Overview's book card is a shortcut back into the reader. Nothing here
 * measures anything: the reading it leads to is written by the same tracker as
 * always, and the screen re-reads its figures when the reader hands control
 * back (main.qml refreshes on becoming active again).
 *
 * The path is checked again rather than trusted — the map it came from may
 * have been built before the book was deleted over USB — and the firmware is
 * left to decide which reader owns the format. */
bool StatsBridge::openBook(const QString &path)
{
    if (path.isEmpty() || !QFileInfo::exists(path)) {
        updateLog(QStringLiteral("open: no file at %1").arg(path));
        return false;
    }
    return inkViewOpenBook(path);
}
