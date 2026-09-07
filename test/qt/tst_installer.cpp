/* The launcher entry. It is the app's one write outside its own directory, it
 * happens on every launch, and the file it edits is the firmware's desktop
 * config — so the whole of it is "change nothing that is already right, and
 * survive a file that is missing, unreadable or not JSON at all". */
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTest>

#include "fixtures.h"
#include "installer.h"
#include "inkview_stub.h"

namespace {

const QString kViewJson = QStringLiteral("/mnt/ext1/system/config/desktop/view.json");
const QString kBackup =
    QStringLiteral("/mnt/ext1/system/config/desktop/view.json.pbstatistics-backup");
const QString kAppId = QStringLiteral("U_pocketbook_statistics");

QByteArray firmwareViewJson()
{
    return R"({
        "applications": {
            "U_reader": { "path": "/ebrmain/bin/reader.app", "title": "Books" }
        },
        "view": {
            "groups": [ { "name": "apps", "apps": ["U_reader"] },
                        { "name": "more", "apps": [] } ]
        }
    })";
}

QJsonObject readJson(const Device &device)
{
    return QJsonDocument::fromJson(device.read(kViewJson)).object();
}

QJsonObject entry(const Device &device)
{
    return readJson(device).value(QStringLiteral("applications")).toObject()
        .value(kAppId).toObject();
}

QJsonArray firstGroupApps(const Device &device)
{
    return readJson(device).value(QStringLiteral("view")).toObject()
        .value(QStringLiteral("groups")).toArray().at(0).toObject()
        .value(QStringLiteral("apps")).toArray();
}

} // namespace

class TestInstaller : public QObject {
    Q_OBJECT

private slots:
    void init()
    {
        InkViewStub::reset();
        device_ = new Device;
    }
    void cleanup() { delete device_; device_ = nullptr; }

    void registersItselfInTheFirstGroup();
    void secondRunChangesNothing();
    void followsTheDeviceLanguage_data();
    void followsTheDeviceLanguage();
    void relabellingDoesNotRegisterTwice();
    void survivesAViewJsonItCannotUse_data();
    void survivesAViewJsonItCannotUse();
    void writesTheIconsOnceAndLeavesThemAlone();

private:
    Device *device_ = nullptr;
};

void TestInstaller::registersItselfInTheFirstGroup()
{
    device_->write(kViewJson, firmwareViewJson());
    ensureRegistered();

    const QJsonObject mine = entry(*device_);
    QCOMPARE(mine.value(QStringLiteral("path")).toString(),
             QStringLiteral("/mnt/ext1/applications/PocketBookStatistics.app"));
    QCOMPARE(mine.value(QStringLiteral("title")).toString(), QStringLiteral("Statistics"));
    /* The launcher resolves the app path absolutely and the icons relative to
     * the storage root; mixing them is required. */
    QCOMPARE(mine.value(QStringLiteral("icon")).toObject()
                 .value(QStringLiteral("path")).toString(),
             QStringLiteral("applications/icons/pocketbook-statistics.bmp"));

    const QJsonArray apps = firstGroupApps(*device_);
    QCOMPARE(apps.size(), 2);
    QCOMPARE(apps.last().toString(), kAppId);
    /* The firmware's own entry is still there. */
    QVERIFY(readJson(*device_).value(QStringLiteral("applications")).toObject()
                .contains(QStringLiteral("U_reader")));
    /* Backed up before the first modification. */
    QCOMPARE(QJsonDocument::fromJson(device_->read(kBackup)),
             QJsonDocument::fromJson(firmwareViewJson()));
}

/* It runs on every launch. A rewrite that changes nothing is a flash write
 * nobody asked for, and a second entry in the group would be a second tile. */
void TestInstaller::secondRunChangesNothing()
{
    device_->write(kViewJson, firmwareViewJson());
    ensureRegistered();
    const QByteArray after = device_->read(kViewJson);

    ensureRegistered();
    QCOMPARE(device_->read(kViewJson), after);
    QCOMPARE(firstGroupApps(*device_).size(), 2);
}

void TestInstaller::followsTheDeviceLanguage_data()
{
    QTest::addColumn<QString>("lang");
    QTest::addColumn<QString>("title");

    QTest::newRow("german") << "de" << "Statistik";
    QTest::newRow("russian") << "ru" << QString::fromUtf8("Статистика");
    QTest::newRow("turkish") << "tr" << QString::fromUtf8("İstatistikler");
    /* The device reports things like "pt_BR"; only the first two letters
     * decide. */
    QTest::newRow("regional") << "pt_BR" << QString::fromUtf8("Estatísticas");
    /* No catalog for it: the tile says what the app does, in English. */
    QTest::newRow("unknown") << "zz" << "Statistics";
}

void TestInstaller::followsTheDeviceLanguage()
{
    QFETCH(QString, lang);
    QFETCH(QString, title);

    InkViewStub::lang = lang;
    device_->write(kViewJson, firmwareViewJson());
    ensureRegistered();

    QCOMPARE(entry(*device_).value(QStringLiteral("title")).toString(), title);
}

/* Switching the reader's language relabels the tile on the next start — and
 * that is the only thing it may do to an entry that is already there. */
void TestInstaller::relabellingDoesNotRegisterTwice()
{
    device_->write(kViewJson, firmwareViewJson());
    ensureRegistered();

    InkViewStub::lang = QStringLiteral("de");
    ensureRegistered();

    QCOMPARE(entry(*device_).value(QStringLiteral("title")).toString(),
             QStringLiteral("Statistik"));
    QCOMPARE(firstGroupApps(*device_).size(), 2);
}

void TestInstaller::survivesAViewJsonItCannotUse_data()
{
    QTest::addColumn<QByteArray>("content");
    QTest::addColumn<bool>("exists");

    QTest::newRow("missing") << QByteArray() << false;
    QTest::newRow("not json") << QByteArrayLiteral("{ this is not json") << true;
    QTest::newRow("an array") << QByteArrayLiteral("[1, 2, 3]") << true;
    QTest::newRow("empty object") << QByteArrayLiteral("{}") << true;
}

/* The app has to start whatever it finds there. A launcher tile is worth
 * nothing next to that. */
void TestInstaller::survivesAViewJsonItCannotUse()
{
    QFETCH(QByteArray, content);
    QFETCH(bool, exists);

    if (exists)
        device_->write(kViewJson, content);

    ensureRegistered();

    if (!exists) {
        QVERIFY2(!QFile::exists(device_->at(kViewJson)),
                 "a view.json that was not there must not be created");
        return;
    }
    if (content == QByteArrayLiteral("{}")) {
        /* Parseable and an object: we are registered, with no group to join. */
        QVERIFY(readJson(*device_).value(QStringLiteral("applications")).toObject()
                    .contains(kAppId));
        return;
    }
    QCOMPARE(device_->read(kViewJson), content);
    QVERIFY2(!QFile::exists(device_->at(kBackup)),
             "nothing was changed, so nothing was backed up");
}

/* The icons travel inside the binary; an update ships new ones. Rewriting
 * identical files on every launch is a flash write for nothing. */
void TestInstaller::writesTheIconsOnceAndLeavesThemAlone()
{
    const QString icon =
        QStringLiteral("/mnt/ext1/applications/icons/pocketbook-statistics.bmp");
    device_->write(kViewJson, firmwareViewJson());
    ensureRegistered();

    QFile resource(QStringLiteral(":/pocketbook-statistics.bmp"));
    QVERIFY(resource.open(QIODevice::ReadOnly));
    const QByteArray written = device_->read(icon);
    QCOMPARE(written, resource.readAll());
    QVERIFY(!written.isEmpty());

    QFile f(device_->at(icon));
    QVERIFY(f.open(QIODevice::ReadWrite));
    const QDateTime old = QDateTime::currentDateTime().addSecs(-3600);
    QVERIFY(f.setFileTime(old, QFileDevice::FileModificationTime));
    f.close();

    ensureRegistered();

    QCOMPARE(device_->read(icon), written);
    QCOMPARE(QFileInfo(device_->at(icon)).lastModified().toSecsSinceEpoch(),
             old.toSecsSinceEpoch());
}

QTEST_GUILESS_MAIN(TestInstaller)
#include "tst_installer.moc"
