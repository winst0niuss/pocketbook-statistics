#include "installer.h"

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include "inkview_bridge.h"

namespace {

// The launcher resolves the app "path" as an absolute path, but the icon
// paths relative to the storage root (/mnt/ext1). Mixing them is required:
// an absolute icon path shows no icon, a relative app path won't launch.
constexpr const char *kAppPath =
    "/mnt/ext1/applications/PocketBookStatistics.app";
constexpr const char *kIconDir = "/mnt/ext1/applications/icons";
constexpr const char *kIconRel =
    "applications/icons/pocketbook-statistics.bmp";
constexpr const char *kIconFocusedRel =
    "applications/icons/pocketbook-statistics_f.bmp";
// Absolute variants for writing the files to disk.
constexpr const char *kIconPath =
    "/mnt/ext1/applications/icons/pocketbook-statistics.bmp";
constexpr const char *kIconFocusedPath =
    "/mnt/ext1/applications/icons/pocketbook-statistics_f.bmp";
constexpr const char *kViewJson =
    "/mnt/ext1/system/config/desktop/view.json";
constexpr const char *kBackup =
    "/mnt/ext1/system/config/desktop/view.json.pbstatistics-backup";
constexpr const char *kAppId = "U_pocketbook_statistics";

/* The launcher label. Every other user-facing string comes from the QML
 * catalogs in qt/qml/i18n/, but this one is written into the firmware's config
 * before any QML engine exists, so it carries its own table — one row per
 * catalog, keep the two in step. The label says what the app does, like the
 * firmware's own tiles do, rather than repeating its name. The device language
 * is read on every launch, so switching the reader's language relabels the
 * tile on the next start. */
struct LauncherName {
    const char *lang;
    const char *title;
};

constexpr LauncherName kLauncherNames[] = {
    {"az", "Statistika"},      {"bg", "Статистика"},
    {"cs", "Statistika"},      {"da", "Statistik"},
    {"de", "Statistik"},       {"el", "Στατιστικά"},
    {"en", "Statistics"},      {"es", "Estadísticas"},
    {"et", "Statistika"},      {"fi", "Tilastot"},
    {"fr", "Statistiques"},    {"hr", "Statistika"},
    {"hu", "Statisztika"},     {"it", "Statistiche"},
    {"kk", "Статистика"},      {"lt", "Statistika"},
    {"lv", "Statistika"},      {"nb", "Statistikk"},
    {"nl", "Statistieken"},    {"no", "Statistikk"},
    {"pl", "Statystyki"},      {"pt", "Estatísticas"},
    {"ro", "Statistici"},      {"ru", "Статистика"},
    {"sk", "Štatistika"},      {"sl", "Statistika"},
    {"sr", "Статистика"},      {"sv", "Statistik"},
    {"tr", "İstatistikler"},   {"uk", "Статистика"},
};

QString launcherTitle()
{
    const QString lang = inkViewLang().left(2).toLower();
    for (const LauncherName &name : kLauncherNames) {
        if (lang == QLatin1String(name.lang))
            return QString::fromUtf8(name.title);
    }
    return QStringLiteral("Statistics");
}

// Copies an embedded resource to a path on the device, but only when the file
// there differs — an app update ships new icons, and rewriting identical ones
// on every launch would be a pointless flash write.
void writeResourceIfChanged(const QString &resource, const QString &dest)
{
    QFile src(resource);
    if (!src.open(QIODevice::ReadOnly))
        return;
    const QByteArray wanted = src.readAll();

    QFile existing(dest);
    if (existing.open(QIODevice::ReadOnly) && existing.readAll() == wanted)
        return;
    existing.close();

    QFile out(dest);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return;
    // A half-written icon is worse than none: the launcher refuses to draw the
    // tile's image and its label slides up into the empty slot. Remove the file
    // so the tile falls back to the default user-app icon instead.
    if (out.write(wanted) != wanted.size() || !out.flush()) {
        out.close();
        QFile::remove(dest);
    }
}

// Adds our launcher entry to view.json. Idempotent, defensive: any failure
// (missing/unparseable/read-only file) is ignored so the app still starts.
void patchViewJson()
{
    const QString viewJsonPath = QLatin1String(kViewJson);
    QFile f(viewJsonPath);
    if (!f.open(QIODevice::ReadOnly))
        return;
    const QByteArray raw = f.readAll();
    f.close();

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(raw, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject())
        return;

    QJsonObject root = doc.object();
    QJsonObject apps = root.value(QStringLiteral("applications")).toObject();
    QJsonObject view = root.value(QStringLiteral("view")).toObject();
    QJsonArray groups = view.value(QStringLiteral("groups")).toArray();
    const QString title = launcherTitle();
    bool changed = false;

    if (apps.contains(QLatin1String(kAppId))) {
        // Registered already: the only thing that can still change is the
        // label, when the reader's language does.
        QJsonObject entry = apps.value(QLatin1String(kAppId)).toObject();
        if (entry.value(QStringLiteral("title")).toString() != title) {
            entry[QStringLiteral("title")] = title;
            apps[QLatin1String(kAppId)] = entry;
            changed = true;
        }
    } else {
        // Back up the original once, before the first modification.
        if (!QFile::exists(QLatin1String(kBackup)))
            QFile::copy(QLatin1String(kViewJson), QLatin1String(kBackup));

        QJsonObject icon;
        icon[QStringLiteral("path")] = QLatin1String(kIconRel);
        QJsonObject iconFocused;
        iconFocused[QStringLiteral("path")] = QLatin1String(kIconFocusedRel);

        QJsonObject entry;
        entry[QStringLiteral("path")] = QLatin1String(kAppPath);
        entry[QStringLiteral("title")] = title;
        entry[QStringLiteral("icon")] = icon;
        entry[QStringLiteral("focused_icon")] = iconFocused;
        apps[QLatin1String(kAppId)] = entry;

        // Put the app id into the first launcher group so it shows up.
        if (!groups.isEmpty()) {
            QJsonObject g0 = groups.at(0).toObject();
            QJsonArray appList = g0.value(QStringLiteral("apps")).toArray();
            appList.append(QLatin1String(kAppId));
            g0[QStringLiteral("apps")] = appList;
            groups.replace(0, g0);
        }
        changed = true;
    }

    if (!changed)
        return;

    root[QStringLiteral("applications")] = apps;
    view[QStringLiteral("groups")] = groups;
    root[QStringLiteral("view")] = view;

    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return;
    f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    f.close();
}

} // namespace

void ensureRegistered()
{
    QDir().mkpath(QLatin1String(kIconDir));
    writeResourceIfChanged(QStringLiteral(":/pocketbook-statistics.bmp"),
                           QLatin1String(kIconPath));
    writeResourceIfChanged(QStringLiteral(":/pocketbook-statistics_f.bmp"),
                           QLatin1String(kIconFocusedPath));
    patchViewJson();
}
