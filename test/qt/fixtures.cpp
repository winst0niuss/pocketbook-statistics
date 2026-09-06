#include "fixtures.h"

#include <QBuffer>
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QImage>

#include <sqlite3.h>

#include "miniz.h"

extern "C" {
#include "tracker.h"
}

namespace {

void exec(const QString &dbPath, const QByteArray &sql)
{
    sqlite3 *db = nullptr;
    if (sqlite3_open(dbPath.toUtf8().constData(), &db) != SQLITE_OK) {
        sqlite3_close(db);
        qFatal("fixture: cannot open %s", qPrintable(dbPath));
    }
    char *err = nullptr;
    if (sqlite3_exec(db, sql.constData(), nullptr, nullptr, &err) != SQLITE_OK)
        qFatal("fixture sql: %s\n%s", err ? err : "?", sql.constData());
    sqlite3_close(db);
}

QByteArray quoted(const QString &s)
{
    return "'" + QString(s).replace(QLatin1Char('\''), QLatin1String("''")).toUtf8() + "'";
}

} // namespace

/* ---- the device --------------------------------------------------------- */

Device::Device()
{
    if (!dir_.isValid())
        qFatal("fixture: no temporary directory");
    qputenv("POCKETBOOK_STATISTICS_ROOT", dir_.path().toUtf8());
    qputenv("POCKETBOOK_STATISTICS_DB", statsDb().toUtf8());
    qputenv("POCKETBOOK_STATISTICS_EXPLORER_DB", explorerDb().toUtf8());
    qputenv("POCKETBOOK_STATISTICS_APP", appPath().toUtf8());
    /* The handover would move the staged binary onto that path and start it. */
    qputenv("POCKETBOOK_STATISTICS_NO_HANDOVER", "1");
    /* app.log is written by the C side, which has a seam of its own. */
    qputenv("POCKETBOOK_STATISTICS_LOG",
            (dir_.path() + QStringLiteral("/app.log")).toUtf8());

    for (const char *sub : {"/mnt/ext1/applications", "/mnt/ext1/system/bin",
                            "/mnt/ext1/system/config/desktop",
                            "/mnt/ext1/system/pocketbook-statistics",
                            "/mnt/ext1/system/cover_chache/hashed", "/books"})
        QDir().mkpath(dir_.path() + QLatin1String(sub));
    write(QStringLiteral("/mnt/ext1/applications/PocketBookStatistics.app"),
          QByteArrayLiteral("\x7f" "ELF pretend binary"));
}

Device::~Device()
{
    qunsetenv("POCKETBOOK_STATISTICS_ROOT");
    qunsetenv("POCKETBOOK_STATISTICS_DB");
    qunsetenv("POCKETBOOK_STATISTICS_EXPLORER_DB");
    qunsetenv("POCKETBOOK_STATISTICS_APP");
    qunsetenv("POCKETBOOK_STATISTICS_NO_HANDOVER");
    qunsetenv("POCKETBOOK_STATISTICS_LOG");
}

void Device::write(const QString &absolute, const QByteArray &content) const
{
    const QString path = at(absolute);
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        qFatal("fixture: cannot write %s", qPrintable(path));
    f.write(content);
    f.close();
}

QByteArray Device::read(const QString &absolute) const
{
    QFile f(at(absolute));
    if (!f.open(QIODevice::ReadOnly))
        return {};
    return f.readAll();
}

/* ---- book files --------------------------------------------------------- */

bool writeZip(const QString &path, const ZipEntries &entries)
{
    QFile::remove(path);
    mz_zip_archive zip;
    memset(&zip, 0, sizeof(zip));
    if (!mz_zip_writer_init_file(&zip, path.toUtf8().constData(), 0))
        return false;
    bool ok = true;
    for (const auto &entry : entries) {
        ok = mz_zip_writer_add_mem(&zip, entry.first.toUtf8().constData(),
                                   entry.second.constData(),
                                   size_t(entry.second.size()),
                                   MZ_BEST_SPEED)
            && ok;
    }
    ok = mz_zip_writer_finalize_archive(&zip) && ok;
    mz_zip_writer_end(&zip);
    return ok;
}

QByteArray pngBytes(int w, int h, const char *colour)
{
    QImage img(w, h, QImage::Format_RGB32);
    img.fill(QColor(QLatin1String(colour)));
    QByteArray out;
    QBuffer buffer(&out);
    buffer.open(QIODevice::WriteOnly);
    img.save(&buffer, "PNG");
    return out;
}

QByteArray epub2(const QByteArray &cover)
{
    const QByteArray container =
        "<?xml version=\"1.0\"?>"
        "<container><rootfiles><rootfile full-path=\"OEBPS/content.opf\"/>"
        "</rootfiles></container>";
    const QByteArray opf =
        "<?xml version=\"1.0\"?><package><metadata>"
        "<meta name=\"cover\" content=\"cover-img\"/></metadata><manifest>"
        "<item id=\"chapter\" href=\"images/chapter.png\" media-type=\"image/png\"/>"
        "<item id=\"cover-img\" href=\"images/front.png\" media-type=\"image/png\"/>"
        "</manifest></package>";
    const QString path = QDir::tempPath() + QStringLiteral("/fixture-epub2.epub");
    writeZip(path, {{QStringLiteral("META-INF/container.xml"), container},
                    {QStringLiteral("OEBPS/content.opf"), opf},
                    /* the chapter illustration is deliberately first and
                     * bigger: only the OPF may decide */
                    {QStringLiteral("OEBPS/images/chapter.png"), pngBytes(60, 60, "#ff0000")},
                    {QStringLiteral("OEBPS/images/front.png"), cover}});
    QFile f(path);
    f.open(QIODevice::ReadOnly);
    const QByteArray bytes = f.readAll();
    f.close();
    QFile::remove(path);
    return bytes;
}

QByteArray epub3(const QByteArray &cover)
{
    const QByteArray container =
        "<?xml version=\"1.0\"?>"
        "<container><rootfiles><rootfile full-path=\"content.opf\"/>"
        "</rootfiles></container>";
    const QByteArray opf =
        "<?xml version=\"1.0\"?><package><manifest>"
        "<item id=\"c\" href=\"front.png\" media-type=\"image/png\""
        " properties=\"cover-image\"/>"
        "</manifest></package>";
    const QString path = QDir::tempPath() + QStringLiteral("/fixture-epub3.epub");
    writeZip(path, {{QStringLiteral("META-INF/container.xml"), container},
                    {QStringLiteral("content.opf"), opf},
                    {QStringLiteral("front.png"), cover}});
    QFile f(path);
    f.open(QIODevice::ReadOnly);
    const QByteArray bytes = f.readAll();
    f.close();
    QFile::remove(path);
    return bytes;
}

QByteArray fb2(const QByteArray &cover)
{
    /* windows-1251, as half of them are, with a cyrillic title in those bytes:
     * the scanner must not try to decode this as UTF-8. */
    QByteArray out = "<?xml version=\"1.0\" encoding=\"windows-1251\"?>\n"
                     "<FictionBook><description><title-info>"
                     "<book-title>\xcf\xf0\xee\xe1\xe0</book-title>"
                     "<coverpage><image l:href=\"#front.png\"/></coverpage>"
                     "</title-info></description>"
                     "<body><p>\xd2\xe5\xea\xf1\xf2</p></body>"
                     "<binary id=\"other.png\" content-type=\"image/png\">";
    out += pngBytes(20, 20, "#ff0000").toBase64();
    out += "</binary><binary id=\"front.png\" content-type=\"image/png\">";
    out += cover.toBase64();
    out += "</binary></FictionBook>";
    return out;
}

QByteArray fb2Zip(const QByteArray &cover)
{
    const QString path = QDir::tempPath() + QStringLiteral("/fixture.fb2.zip");
    writeZip(path, {{QStringLiteral("book.fb2"), fb2(cover)}});
    QFile f(path);
    f.open(QIODevice::ReadOnly);
    const QByteArray bytes = f.readAll();
    f.close();
    QFile::remove(path);
    return bytes;
}

QByteArray cbz(const QList<QPair<QString, QByteArray>> &pages)
{
    const QString path = QDir::tempPath() + QStringLiteral("/fixture.cbz");
    writeZip(path, pages);
    QFile f(path);
    f.open(QIODevice::ReadOnly);
    const QByteArray bytes = f.readAll();
    f.close();
    QFile::remove(path);
    return bytes;
}

/* ---- databases ---------------------------------------------------------- */

void createExplorer(const QString &path)
{
    QFile::remove(path);
    exec(path,
         "CREATE TABLE books_impl (id INTEGER PRIMARY KEY, title TEXT, author TEXT);"
         "CREATE TABLE books_settings (bookid INTEGER, profileid INTEGER,"
         "  position TEXT, position_ts INTEGER, cpage INTEGER, npage INTEGER,"
         "  opentime INTEGER, completed INTEGER, completed_ts INTEGER);"
         "CREATE TABLE books_fast_hashes (fast_hash BLOB PRIMARY KEY, book_id INTEGER);"
         "CREATE TABLE files (book_id INTEGER, storageid INTEGER, filename TEXT,"
         "  folder_id INTEGER, modification_time INTEGER, fast_hash BLOB);"
         "CREATE TABLE folders (id INTEGER PRIMARY KEY, name TEXT);"
         "CREATE TABLE storages (id INTEGER PRIMARY KEY);"
         "INSERT INTO storages VALUES (1);");
}

void insertBook(const QString &explorerPath, const BookRow &book)
{
    QByteArray sql;
    sql += "INSERT INTO books_impl VALUES (" + QByteArray::number(book.id) + ","
        + quoted(book.title) + "," + quoted(book.author) + ");";
    sql += "INSERT INTO books_settings VALUES (" + QByteArray::number(book.id)
        + ",1,'p'," + QByteArray::number(book.positionTs) + ","
        + QByteArray::number(book.cpage) + "," + QByteArray::number(book.npage) + ","
        + QByteArray::number(book.opentime) + "," + QByteArray::number(book.completed)
        + "," + QByteArray::number(book.completedTs) + ");";
    if (!book.hash.isEmpty())
        sql += "INSERT INTO books_fast_hashes VALUES (x'" + book.hash.toUtf8() + "',"
            + QByteArray::number(book.id) + ");";
    if (!book.folder.isEmpty()) {
        const qint64 folderId = qHash(book.folder) % 100000;
        sql += "INSERT OR IGNORE INTO folders VALUES (" + QByteArray::number(folderId)
            + "," + quoted(book.folder) + ");";
        sql += "INSERT INTO files VALUES (" + QByteArray::number(book.id) + ","
            + QByteArray::number(book.storageId) + "," + quoted(book.filename) + ","
            + QByteArray::number(folderId) + ","
            + QByteArray::number(book.modificationTime) + ",x'"
            + book.hash.toUtf8() + "');";
    }
    exec(explorerPath, sql);
}

void createStats(const QString &path)
{
    QFile::remove(path);
    tracker t;
    if (tracker_init(&t, path.toUtf8().constData(), "") != 0)
        qFatal("fixture: tracker_init failed for %s", qPrintable(path));
    tracker_close(&t);
}

void insertSession(const QString &statsPath, const SessionRow &s)
{
    exec(statsPath,
         "INSERT INTO sessions (book_id,start_time,end_time,active_seconds,"
         " pages_start,pages_end,pages_read,recovered) VALUES ("
         + QByteArray::number(s.bookId) + "," + QByteArray::number(s.start) + ","
         + QByteArray::number(s.end) + "," + QByteArray::number(s.activeSeconds)
         + ",0," + QByteArray::number(s.pagesRead) + ","
         + QByteArray::number(s.pagesRead) + ","
         + QByteArray::number(s.recovered) + ");");
}

void insertOwnBook(const QString &statsPath, qint64 bookId, const QString &title,
                   const QString &author, const QString &coverKey)
{
    exec(statsPath,
         "INSERT OR REPLACE INTO books (book_id,title,author,cover,cpage,npage,"
         " completed,last_seen) VALUES (" + QByteArray::number(bookId) + ","
         + quoted(title) + "," + quoted(author) + "," + quoted(coverKey)
         + ",0,0,0,0);");
}

void setMeta(const QString &statsPath, const QString &key, qint64 value)
{
    exec(statsPath, "INSERT OR REPLACE INTO meta (key,value) VALUES ("
         + quoted(key) + "," + QByteArray::number(value) + ");");
}

qint64 todayMidnight()
{
    return QDateTime(QDate::currentDate(), QTime(0, 0)).toSecsSinceEpoch();
}

qint64 todayNoon()
{
    return QDateTime(QDate::currentDate(), QTime(12, 0)).toSecsSinceEpoch();
}
