#pragma once

#include <QObject>
#include <QString>
#include <QVariantMap>

struct sqlite3;

/* Bridge between QML and the existing C modules (tracker/stats_db). */
class StatsBridge : public QObject {
    Q_OBJECT

public:
    explicit StatsBridge(QObject *parent = nullptr);
    ~StatsBridge() override;

    Q_INVOKABLE QVariantMap overall();
    Q_INVOKABLE QVariantMap currentBook();
    Q_INVOKABLE QVariantMap month(int year, int mon);
    /* A whole year as one map, for the streak screen: `days` holds one entry
     * per day from 1 January (0 not read, 1 read), beside the two streaks and
     * the day tracking started. */
    Q_INVOKABLE QVariantMap year(int y);
    /* Hands a book back to the firmware's reader. `path` is the `filePath` of
     * a currentBook() map — a file that was on disk when the map was built.
     * False means it is not there any more, or the firmware has no OpenBook;
     * either way nothing happened and the screen stays. */
    Q_INVOKABLE bool openBook(const QString &path);

private:
    /* Reads the firmware's current state and folds it into our DB before a
     * screen aggregates it, so a tab never shows what the daemon last happened
     * to poll. */
    void catchUp();

    sqlite3 *db_ = nullptr;
};
