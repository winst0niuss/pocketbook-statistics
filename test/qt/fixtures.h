#pragma once

#include <QByteArray>
#include <QDir>
#include <QList>
#include <QPair>
#include <QString>
#include <QTemporaryDir>

/* A device in a temporary directory.
 *
 * Everything the app writes goes through devicePath(), so pointing
 * POCKETBOOK_STATISTICS_ROOT at a temp dir gives a test the whole of
 * /mnt/ext1 to itself — applications/, system/config/, system/bin/, the cover
 * caches and the two databases. Torn down with the directory.
 */
class Device {
public:
    Device();
    ~Device();

    QString root() const { return dir_.path(); }
    /* A device-absolute path inside this root: at("/mnt/ext1/…"). */
    QString at(const QString &absolute) const { return dir_.path() + absolute; }
    QString statsDb() const { return at(QStringLiteral("/stats.db")); }
    QString explorerDb() const { return at(QStringLiteral("/explorer-3.db")); }
    /* Where the installed binary pretends to be. */
    QString appPath() const { return at(QStringLiteral("/mnt/ext1/applications/PocketBookStatistics.app")); }

    void write(const QString &absolute, const QByteArray &content) const;
    QByteArray read(const QString &absolute) const;

private:
    QTemporaryDir dir_;
};

/* ---- book files -------------------------------------------------------- */

using ZipEntries = QList<QPair<QString, QByteArray>>;

/* A zip at `path`, entries in the order given (miniz writes them in order, so
 * this is also what a "first page" test depends on). */
bool writeZip(const QString &path, const ZipEntries &entries);

/* A PNG with one recognisable colour, so a test can tell which image came back
 * out of a cover lookup. */
QByteArray pngBytes(int w, int h, const char *colour = "#204080");

/* Minimal but real books. Each returns the bytes; the caller decides where they
 * land, because most of these tests care about what happens when the file is
 * no longer there. */
QByteArray epub2(const QByteArray &cover);   /* <meta name="cover"> */
QByteArray epub3(const QByteArray &cover);   /* properties="cover-image" */
QByteArray fb2(const QByteArray &cover);     /* bare .fb2, windows-1251 header */
QByteArray fb2Zip(const QByteArray &cover);
QByteArray cbz(const QList<QPair<QString, QByteArray>> &pages);

/* ---- databases ---------------------------------------------------------- */

/* explorer-3.db as the firmware keeps it: the tables and columns this app
 * reads, and nothing else. Empty — the seeders below fill it. */
void createExplorer(const QString &path);

struct BookRow {
    qint64 id = 1;
    QString title;
    QString author;
    QString hash;              /* hex, as books_fast_hashes stores it */
    /* books_settings */
    qint64 opentime = 0;
    qint64 positionTs = 0;
    int cpage = 0;
    int npage = 0;
    int completed = 0;
    qint64 completedTs = 0;
    /* files/folders: a book still on the device has one, a deleted one none */
    QString folder;            /* empty = no files row at all */
    QString filename;
    int storageId = 1;
    qint64 modificationTime = 0;
};

void insertBook(const QString &explorerPath, const BookRow &book);

/* Our own statistics.db, created by the tracker itself so the schema is the one
 * that ships. */
void createStats(const QString &path);

struct SessionRow {
    qint64 bookId = 1;
    qint64 start = 0;
    qint64 end = 0;
    qint64 activeSeconds = 0;
    int pagesRead = 0;
    int recovered = 0;
};

void insertSession(const QString &statsPath, const SessionRow &session);
/* The `books` table our own DB keeps, which is what the calendar joins titles
 * and cover keys from. */
void insertOwnBook(const QString &statsPath, qint64 bookId, const QString &title,
                   const QString &author, const QString &coverKey);
void setMeta(const QString &statsPath, const QString &key, qint64 value);

/* Local midnight of the day the test runs in, and noon — the day-level SQL
 * groups by localtime, so a fixture built from a fixed epoch would land on a
 * different day depending on the machine. */
qint64 todayMidnight();
qint64 todayNoon();
