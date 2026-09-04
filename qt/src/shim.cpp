#include "shim.h"

#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QStringList>

#include "update_log.h"

namespace {

constexpr const char *kScriptName = "pbstatistics-open.app";
constexpr const char *kScriptPath = "/mnt/ext1/system/bin/pbstatistics-open.app";
constexpr const char *kBinDir = "/mnt/ext1/system/bin";
constexpr const char *kUserExt = "/mnt/ext1/system/config/extensions.cfg";
constexpr const char *kUserExtBackup =
    "/mnt/ext1/system/config/extensions.cfg.pbstatistics-backup";
/* The firmware's own table. Read-only, and read only to learn which reader
 * owns a format — never written. */
constexpr const char *kSysExt = "/ebrmain/config/extensions.cfg";

/* Formats worth intercepting: the ones a reader actually reads. Leaving the
 * rest alone keeps the blast radius small — every entry here is a format that
 * stops opening if the shim is broken. */
const QStringList &formats()
{
    static const QStringList f{QStringLiteral("epub"), QStringLiteral("fb2"),
                               QStringLiteral("pdf")};
    return f;
}

QStringList readLines(const char *path)
{
    QFile f(QString::fromLatin1(path));
    if (!f.open(QIODevice::ReadOnly))
        return {};
    const QString text = QString::fromUtf8(f.readAll());
    f.close();
    return text.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
}

/* Entries look like
 *     epub:@EPUB_file:1:reader.app,other.app:ICON_EPUB
 * — extension, internal name, priority, applications, icon. */
QString entryFor(const QStringList &lines, const QString &ext)
{
    const QString prefix = ext + QLatin1Char(':');
    for (const QString &line : lines) {
        if (line.startsWith(prefix, Qt::CaseInsensitive))
            return line;
    }
    return QString();
}

/* Our name, then whatever was there before. Anything already listed stays, so
 * "open with" keeps offering the other readers — overwriting that list is how
 * KOReader once made the firmware's own viewers disappear. */
QString withShimFirst(const QString &entry, const QString &ext)
{
    QStringList fields = entry.split(QLatin1Char(':'));
    if (fields.size() < 5) {
        const QString upper = ext.toUpper();
        fields = QStringList{ext,
                             QLatin1Char('@') + upper + QStringLiteral("_file"),
                             QStringLiteral("1"),
                             QString(),
                             QStringLiteral("ICON_") + upper};
    }
    QStringList apps = fields[3].split(QLatin1Char(','), Qt::SkipEmptyParts);
    apps.removeAll(QString::fromLatin1(kScriptName));
    apps.prepend(QString::fromLatin1(kScriptName));
    fields[3] = apps.join(QLatin1Char(','));
    return fields.join(QLatin1Char(':'));
}

QString withoutShim(const QString &entry)
{
    QStringList fields = entry.split(QLatin1Char(':'));
    if (fields.size() < 5)
        return QString();
    QStringList apps = fields[3].split(QLatin1Char(','), Qt::SkipEmptyParts);
    apps.removeAll(QString::fromLatin1(kScriptName));
    if (apps.isEmpty())
        return QString(); /* the entry existed only for us: drop it */
    fields[3] = apps.join(QLatin1Char(','));
    return fields.join(QLatin1Char(':'));
}

bool writeLines(const char *path, const QStringList &lines)
{
    QFile f(QString::fromLatin1(path));
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;
    const QByteArray text = (lines.join(QLatin1Char('\n')) + QLatin1Char('\n')).toUtf8();
    const bool ok = f.write(text) == text.size() && f.flush();
    f.close();
    return ok;
}

} // namespace

Shim::Shim(QObject *parent) : QObject(parent) {}

/* Every format, not any: a device that was set up by hand — or by a version
 * that intercepted fewer formats — must read as "off", so pressing the button
 * completes it instead of appearing to be done already. */
bool Shim::installed() const
{
    if (!QFileInfo::exists(QString::fromLatin1(kScriptPath)))
        return false;
    const QStringList lines = readLines(kUserExt);
    for (const QString &ext : formats()) {
        const QString entry = entryFor(lines, ext);
        if (!entry.contains(QString::fromLatin1(kScriptName)))
            return false;
    }
    return true;
}

void Shim::refresh()
{
    /* Deliberately not installed(): that also demands an entry for every
     * format, and a device set up by hand — or by an older build — may name
     * only one. The script on disk is ours whatever the entries say, and
     * leaving a stale copy there is how a fix to it never reaches the reader. */
    if (!QFileInfo::exists(QString::fromLatin1(kScriptPath)))
        return;
    QFile src(QStringLiteral(":/shim/open-book.sh"));
    QFile installedFile(QString::fromLatin1(kScriptPath));
    if (!src.open(QIODevice::ReadOnly) || !installedFile.open(QIODevice::ReadOnly))
        return;
    const QByteArray shipped = src.readAll();
    const bool same = installedFile.readAll() == shipped;
    installedFile.close();
    if (same)
        return;
    /* Same write as install(), minus the extensions.cfg work: the entries are
     * already there and name a script that is about to be replaced in place. */
    QFile out(QString::fromLatin1(kScriptPath));
    if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return;
    if (out.write(shipped) != shipped.size() || !out.flush()) {
        out.close();
        updateLog(QStringLiteral("shim: refresh failed, leaving the old script"));
        return;
    }
    out.close();
    QFile::setPermissions(QString::fromLatin1(kScriptPath),
                          QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner
                              | QFile::ReadGroup | QFile::ExeGroup
                              | QFile::ReadOther | QFile::ExeOther);
    updateLog(QStringLiteral("shim: script refreshed"));
}

bool Shim::install()
{
    QDir().mkpath(QString::fromLatin1(kBinDir));

    QFile src(QStringLiteral(":/shim/open-book.sh"));
    if (!src.open(QIODevice::ReadOnly)) {
        updateLog(QStringLiteral("shim: resource missing"));
        return false;
    }
    const QByteArray script = src.readAll();
    src.close();

    QFile out(QString::fromLatin1(kScriptPath));
    if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        updateLog(QStringLiteral("shim: cannot write %1").arg(QString::fromLatin1(kScriptPath)));
        return false;
    }
    const bool written = out.write(script) == script.size() && out.flush();
    out.close();
    if (!written) {
        /* A half-written script would be run by the firmware and would not
         * reach its handover line, which means a book that does not open. */
        QFile::remove(QString::fromLatin1(kScriptPath));
        updateLog(QStringLiteral("shim: short write, removed"));
        return false;
    }
    QFile::setPermissions(QString::fromLatin1(kScriptPath),
                          QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner
                              | QFile::ReadGroup | QFile::ExeGroup
                              | QFile::ReadOther | QFile::ExeOther);

    if (!QFile::exists(QString::fromLatin1(kUserExtBackup)))
        QFile::copy(QString::fromLatin1(kUserExt), QString::fromLatin1(kUserExtBackup));

    QStringList lines = readLines(kUserExt);
    const QStringList sysLines = readLines(kSysExt);
    for (const QString &ext : formats()) {
        QString entry = entryFor(lines, ext);
        const bool fromUser = !entry.isEmpty();
        if (!fromUser)
            entry = entryFor(sysLines, ext); /* empty is fine: fabricated below */
        const QString patched = withShimFirst(entry, ext);
        if (fromUser) {
            for (QString &line : lines) {
                if (line.startsWith(ext + QLatin1Char(':'), Qt::CaseInsensitive)) {
                    line = patched;
                    break;
                }
            }
        } else {
            lines.append(patched);
        }
    }

    if (!writeLines(kUserExt, lines)) {
        QFile::remove(QString::fromLatin1(kScriptPath));
        updateLog(QStringLiteral("shim: cannot write extensions.cfg, rolled back"));
        return false;
    }
    updateLog(QStringLiteral("shim: installed for %1").arg(formats().join(QLatin1Char(','))));
    return true;
}

bool Shim::remove()
{
    QStringList lines = readLines(kUserExt);
    QStringList kept;
    for (const QString &line : lines) {
        if (!line.contains(QString::fromLatin1(kScriptName))) {
            kept.append(line);
            continue;
        }
        const QString stripped = withoutShim(line);
        if (!stripped.isEmpty())
            kept.append(stripped);
    }
    const bool cfgOk = writeLines(kUserExt, kept);
    const bool fileOk = QFile::remove(QString::fromLatin1(kScriptPath));
    updateLog(QStringLiteral("shim: removed (cfg %1, script %2)")
                  .arg(cfgOk ? QStringLiteral("ok") : QStringLiteral("failed"),
                       fileOk ? QStringLiteral("ok") : QStringLiteral("failed")));
    return cfgOk;
}
