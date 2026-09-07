#include "shim.h"

#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QStringList>

#include "update_log.h"

extern "C" {
#include "daemon.h"
}

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
/* Written the first time the app decides about the shim on its own. Its
 * presence, not its content, is the whole record. */
constexpr const char *kDefaultMarker = STATS_DIR "/shim-default";

/* The three a reader is for. These are intercepted whether or not the device's
 * table names them: install() fabricates an entry where there is none, and the
 * shim falls back to the readers a PocketBook ships. */
const QStringList &baseFormats()
{
    static const QStringList f{QStringLiteral("epub"), QStringLiteral("fb2"),
                               QStringLiteral("pdf")};
    return f;
}

/* Every other extension PocketBook's own readers open, across models. The list
 * is deliberately generous, because formats() keeps only the ones this device's
 * table actually names — an extension no firmware here knows about costs
 * nothing, and one the table routes somewhere that is not a reader (music,
 * images, fonts, firmware images) is not in the list at all. That is what keeps
 * the blast radius bounded: every format intercepted is one that stops opening
 * if the shim is broken. `acsm` is left out on purpose — it is a fulfilment
 * token, not a book; what it downloads is an epub or a pdf, and those are
 * already here. */
const QStringList &otherFormats()
{
    static const QStringList f{
        QStringLiteral("fb3"),  QStringLiteral("fbz"),  QStringLiteral("zip"),
        QStringLiteral("djvu"), QStringLiteral("djv"),  QStringLiteral("txt"),
        QStringLiteral("rtf"),  QStringLiteral("doc"),  QStringLiteral("docx"),
        QStringLiteral("html"), QStringLiteral("htm"),  QStringLiteral("chm"),
        QStringLiteral("mobi"), QStringLiteral("prc"),  QStringLiteral("azw"),
        QStringLiteral("azw3"), QStringLiteral("pdb"),  QStringLiteral("tcr"),
        QStringLiteral("oeb"),  QStringLiteral("cbz"),  QStringLiteral("cbr"),
        QStringLiteral("cbt")};
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

/* Every format this device can open: the base three, plus each of the others
 * the firmware's table — or the user's — already routes somewhere. Fabricating
 * an entry for a format nothing here handles would put our script in front of
 * an empty list, and the shim would then have to guess which reader the
 * firmware meant; skipping it leaves that file exactly as it was. */
QStringList formats()
{
    QStringList out = baseFormats();
    const QStringList userLines = readLines(kUserExt);
    const QStringList sysLines = readLines(kSysExt);
    for (const QString &ext : otherFormats()) {
        if (!entryFor(userLines, ext).isEmpty()
            || !entryFor(sysLines, ext).isEmpty())
            out.append(ext);
    }
    return out;
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

/* At least one entry names us, which is what "the shim is on" means whatever
 * else may be missing. The top-up in refresh() needs this rather than
 * installed(), which is all-or-nothing: a user who has just switched the shim
 * off must not have the entries written back at the next start. */
bool anyEntryNamesUs()
{
    for (const QString &line : readLines(kUserExt)) {
        if (line.contains(QString::fromLatin1(kScriptName)))
            return true;
    }
    return false;
}

/* The extensions.cfg half of an install: the backup, then our name at the front
 * of the application list for every format this device can open. Idempotent —
 * withShimFirst() takes us out before it puts us back — so it is safe to run
 * over an install that already covers some of them. */
bool writeEntries()
{
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
    return writeLines(kUserExt, lines);
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

/* The script half of refresh(): replaces the installed copy when the app now
 * ships a different one. */
static void refreshScript()
{
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

void Shim::refresh()
{
    /* Deliberately not installed(): that also demands an entry for every
     * format, and a device set up by hand — or by an older build — may name
     * only one. The script on disk is ours whatever the entries say, and
     * leaving a stale copy there is how a fix to it never reaches the reader. */
    if (!QFileInfo::exists(QString::fromLatin1(kScriptPath)))
        return;
    refreshScript();
    /* The entries are as much part of the shim as the script is. A build that
     * reads more formats than the one which installed it would otherwise leave
     * the new ones to open without a daemon behind them, and the About switch
     * would read as off — asking the user to turn on again something they had
     * already turned on, for a change they never made. */
    if (anyEntryNamesUs() && !installed() && writeEntries())
        updateLog(QStringLiteral("shim: entries brought up to date"));
}

/* Tracking has to work without being switched on. Sessions are derived from
 * the firmware's own timestamps, but only while something of ours is running,
 * and nothing of ours starts at boot: without the shim, a reader that has been
 * switched off measures nothing at all until the app is next opened by hand. So
 * the shim goes in on the first run rather than waiting to be found on the
 * About screen — someone who installs a reading tracker and sees an empty day
 * after an evening of reading has no way of guessing there was a switch.
 *
 * Once, ever, and the marker is written before the attempt: a failure is not
 * retried at every launch, and a user who turns the shim back off is not
 * overruled the next time the app starts. */
void Shim::enableByDefault()
{
    const QString marker = QString::fromLatin1(kDefaultMarker);
    if (QFileInfo::exists(marker))
        return;
    QDir().mkpath(QFileInfo(marker).path());
    QFile stamp(marker);
    if (stamp.open(QIODevice::WriteOnly))
        stamp.close();

    if (installed())
        return;
    updateLog(QStringLiteral("shim: switching autostart on for the first run"));
    install();
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

    if (!writeEntries()) {
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
