/* The update path. Nothing here talks to GitHub — inkview_stub answers the two
 * firmware calls — so what is under test is what the app does with the answer:
 * which release it picks out of a list, when it refuses one, and what it leaves
 * on disk for the handover. */
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTest>

#include "fixtures.h"
#include "inkview_bridge.h"
#include "inkview_stub.h"
#include "updater.h"

namespace {

const QString kUpdateDir = QStringLiteral("/mnt/ext1/system/pocketbook-statistics/update");
const QString kPrereleaseFlag =
    QStringLiteral("/mnt/ext1/system/pocketbook-statistics/prerelease");

QByteArray release(const QString &tag, bool withAsset = true, qint64 size = 700000)
{
    QJsonObject asset;
    asset[QStringLiteral("name")] = QStringLiteral("PocketBookStatistics.zip");
    asset[QStringLiteral("browser_download_url")] =
        QStringLiteral("https://example.invalid/") + tag + QStringLiteral(".zip");
    asset[QStringLiteral("size")] = double(size);

    QJsonObject r;
    r[QStringLiteral("tag_name")] = tag;
    if (withAsset)
        r[QStringLiteral("assets")] = QJsonArray{asset};
    else
        r[QStringLiteral("assets")] = QJsonArray{};
    return QJsonDocument(r).toJson(QJsonDocument::Compact);
}

QByteArray releaseList(const QStringList &tags)
{
    QJsonArray list;
    for (const QString &tag : tags)
        list.append(QJsonDocument::fromJson(release(tag)).object());
    return QJsonDocument(list).toJson(QJsonDocument::Compact);
}

/* A release zip: one PocketBookStatistics.app, big enough to pass for a
 * binary, starting with the ELF magic the updater checks for. */
QByteArray releaseZip(const QString &path, const QByteArray &magic = QByteArrayLiteral("\x7f" "ELF"),
                      int size = 400 * 1024, const QString &member = QStringLiteral("PocketBookStatistics.app"))
{
    QByteArray binary = magic;
    binary.append(size - binary.size(), 'x');
    writeZip(path, {{member, binary}});
    QFile f(path);
    f.open(QIODevice::ReadOnly);
    const QByteArray bytes = f.readAll();
    f.close();
    return bytes;
}

} // namespace

class TestUpdater : public QObject {
    Q_OBJECT

private slots:
    void init()
    {
        InkViewStub::reset();
        device_ = new Device;
        updater_ = new Updater;
    }
    void cleanup()
    {
        delete updater_;
        delete device_;
        updater_ = nullptr;
        device_ = nullptr;
    }

    void offlineSaysSoWithoutAsking();
    void ownVersionIsUpToDate();
    void aNewerReleaseIsOffered();
    void aNewerReleaseWithoutABuildIsAnError();
    void garbageFromGitHubIsAnError_data();
    void garbageFromGitHubIsAnError();
    void stableChannelAsksForTheLatestRelease();
    void prereleaseChannelKeepsTheHighestTagNotTheFirst();
    void theCheckGivesUpQuicklyAndTheDownloadDoesNot();
    void installStagesBesideTheBinaryAndWritesTheHandover();
    void installRefusesAShortDownload();
    void installRefusesAZipWithoutABinary_data();
    void installRefusesAZipWithoutABinary();
    void aFailedTransferIsToldApartFromAFirmwareThatCannotDownload_data();
    void aFailedTransferIsToldApartFromAFirmwareThatCannotDownload();
    void installNeedsTheNetworkOfItsOwn();
    void theLogIsWhatTheScreenShowsWhenARunFails();

private:
    void answer(const QByteArray &body, int rc = 0)
    {
        InkViewStub::downloads.append(InkViewStub::Download{rc, body});
    }

    Device *device_ = nullptr;
    Updater *updater_ = nullptr;
};

/* Associated but offline is the common case — a router with no uplink — and it
 * must not cost the user a two-minute freeze to find out. */
void TestUpdater::offlineSaysSoWithoutAsking()
{
    InkViewStub::networkUp = false;

    updater_->check();

    QCOMPARE(updater_->state(), QStringLiteral("error"));
    QCOMPARE(updater_->errorKey(), QStringLiteral("update.errNoNetwork"));
    QVERIFY(InkViewStub::requestedUrls.isEmpty());
}

void TestUpdater::ownVersionIsUpToDate()
{
    answer(release(QStringLiteral("v") + updater_->currentVersion()));

    updater_->check();

    QCOMPARE(updater_->state(), QStringLiteral("uptodate"));
}

void TestUpdater::aNewerReleaseIsOffered()
{
    answer(release(QStringLiteral("v99.0.0")));

    updater_->check();

    QCOMPARE(updater_->state(), QStringLiteral("available"));
    QCOMPARE(updater_->latestVersion(), QStringLiteral("v99.0.0"));
}

/* A release that ships no binary we can install is not an offer. */
void TestUpdater::aNewerReleaseWithoutABuildIsAnError()
{
    answer(release(QStringLiteral("v99.0.0"), false));

    updater_->check();

    QCOMPARE(updater_->state(), QStringLiteral("error"));
    QCOMPARE(updater_->errorKey(), QStringLiteral("update.errNoAsset"));
}

void TestUpdater::garbageFromGitHubIsAnError_data()
{
    QTest::addColumn<QByteArray>("body");
    QTest::newRow("empty") << QByteArray();
    QTest::newRow("html error page") << QByteArrayLiteral("<html>502</html>");
    QTest::newRow("no tag") << QByteArrayLiteral("{\"assets\":[]}");
    QTest::newRow("empty list") << QByteArrayLiteral("[]");
    QTest::newRow("only drafts")
        << QByteArrayLiteral("[{\"draft\":true,\"tag_name\":\"v99.0.0\"}]");
}

void TestUpdater::garbageFromGitHubIsAnError()
{
    QFETCH(QByteArray, body);
    if (body.startsWith('[') || body.contains("draft"))
        device_->write(kPrereleaseFlag, QByteArray());
    answer(body);

    updater_->check();

    QCOMPARE(updater_->state(), QStringLiteral("error"));
    QCOMPARE(updater_->errorKey(), QStringLiteral("update.errResponse"));
}

/* Ordinary readers must never be offered a candidate: /releases/latest skips
 * pre-releases, and only a device carrying the marker file asks for the list. */
void TestUpdater::stableChannelAsksForTheLatestRelease()
{
    answer(release(QStringLiteral("v99.0.0")));

    updater_->check();

    QCOMPARE(InkViewStub::requestedUrls.size(), 1);
    QVERIFY2(InkViewStub::requestedUrls.first().endsWith(QStringLiteral("/releases/latest")),
             qPrintable(InkViewStub::requestedUrls.first()));
}

/* GitHub sorts /releases by tag name as text, so a run of candidates comes back
 * as rc9, rc8 … rc2, rc11, rc10, rc1: the newest two sit in the middle. Taking
 * the head of that list offered a device on rc9 its own version. */
void TestUpdater::prereleaseChannelKeepsTheHighestTagNotTheFirst()
{
    device_->write(kPrereleaseFlag, QByteArray());
    /* A version this app will never carry, so the assertion is about the
     * ordering and not about what VERSION happens to say today. */
    answer(releaseList({QStringLiteral("v99.0.0-rc9"), QStringLiteral("v99.0.0-rc8"),
                        QStringLiteral("v99.0.0-rc11"), QStringLiteral("v99.0.0-rc10"),
                        QStringLiteral("v99.0.0-rc1")}));

    updater_->check();

    QVERIFY2(InkViewStub::requestedUrls.first().contains(QStringLiteral("/releases?")),
             qPrintable(InkViewStub::requestedUrls.first()));
    QCOMPARE(updater_->latestVersion(), QStringLiteral("v99.0.0-rc11"));
    QCOMPARE(updater_->state(), QStringLiteral("available"));
}

/* Both transfers stop the UI thread, so the version check is given a fifth of
 * the release's patience: it is a few hundred bytes and a hung one is a frozen
 * screen. */
void TestUpdater::theCheckGivesUpQuicklyAndTheDownloadDoesNot()
{
    answer(release(QStringLiteral("v99.0.0"), true, 0));
    updater_->check();
    const int checkTimeout = InkViewStub::requestedTimeouts.first();

    const QString zip = device_->at(QStringLiteral("/release.zip"));
    answer(releaseZip(zip));
    updater_->install();

    QCOMPARE(InkViewStub::requestedTimeouts.size(), 2);
    QVERIFY2(InkViewStub::requestedTimeouts.last() > checkTimeout,
             "the release is worth waiting for, the check is not");
}

/* A running binary cannot overwrite itself: the new one is staged beside the
 * installed file — the same mount, since mv is only atomic within one — and a
 * generated script does the swap once this process is gone. */
void TestUpdater::installStagesBesideTheBinaryAndWritesTheHandover()
{
    const QByteArray zip = releaseZip(device_->at(QStringLiteral("/release.zip")));
    answer(release(QStringLiteral("v99.0.0"), true, zip.size()));
    updater_->check();
    QCOMPARE(updater_->state(), QStringLiteral("available"));

    answer(zip);
    updater_->install();

    QCOMPARE(updater_->state(), QStringLiteral("ready"));
    const QString staged = device_->appPath() + QStringLiteral(".new");
    QVERIFY2(QFile::exists(staged), qPrintable(staged));
    QVERIFY(QFileInfo(staged).permission(QFile::ExeOwner));
    QFile f(staged);
    QVERIFY(f.open(QIODevice::ReadOnly));
    QCOMPARE(f.read(4), QByteArrayLiteral("\x7f" "ELF"));
    f.close();

    const QByteArray script = device_->read(kUpdateDir + QStringLiteral("/apply.sh"));
    QVERIFY2(script.contains(staged.toUtf8()), script.constData());
    QVERIFY2(script.contains(device_->appPath().toUtf8()), script.constData());
    /* It waits for this process, stops the daemon (same ELF), then moves. */
    QVERIFY(script.contains("kill -0 " + QByteArray::number(QCoreApplication::applicationPid())));
    QVERIFY(script.contains("mv -f"));

    /* The zip itself is not left behind on a device with 4 MB free. */
    QVERIFY(!QFile::exists(device_->at(kUpdateDir + QStringLiteral("/PocketBookStatistics.zip"))));
}

/* Anything short of the size GitHub declared is a proxy error page or a
 * truncated transfer, and nothing may be swapped in from it. */
void TestUpdater::installRefusesAShortDownload()
{
    const QByteArray zip = releaseZip(device_->at(QStringLiteral("/release.zip")));
    answer(release(QStringLiteral("v99.0.0"), true, zip.size() + 1));
    updater_->check();

    answer(zip);
    updater_->install();

    QCOMPARE(updater_->state(), QStringLiteral("error"));
    QCOMPARE(updater_->errorKey(), QStringLiteral("update.errCorrupt"));
    QVERIFY(!QFile::exists(device_->appPath() + QStringLiteral(".new")));
}

void TestUpdater::installRefusesAZipWithoutABinary_data()
{
    QTest::addColumn<QByteArray>("zip");
    const QString path = QDir::tempPath() + QStringLiteral("/updater-fixture.zip");

    QTest::newRow("not a zip") << QByteArrayLiteral("nonsense, not an archive");
    QTest::newRow("no .app inside")
        << releaseZip(path, QByteArrayLiteral("\x7f" "ELF"), 400 * 1024,
                      QStringLiteral("README.md"));
    QTest::newRow("not an ELF")
        << releaseZip(path, QByteArrayLiteral("MZ.."), 400 * 1024);
    QTest::newRow("far too small")
        << releaseZip(path, QByteArrayLiteral("\x7f" "ELF"), 1024);
}

void TestUpdater::installRefusesAZipWithoutABinary()
{
    QFETCH(QByteArray, zip);
    answer(release(QStringLiteral("v99.0.0"), true, zip.size()));
    updater_->check();

    answer(zip);
    updater_->install();

    QCOMPARE(updater_->state(), QStringLiteral("error"));
    QCOMPARE(updater_->errorKey(), QStringLiteral("update.errCorrupt"));
    QVERIFY(!QFile::exists(device_->appPath() + QStringLiteral(".new")));
}

/* A firmware that exports no usable download function at all is a different
 * message from a transfer that failed: one of them is worth trying again. */
void TestUpdater::aFailedTransferIsToldApartFromAFirmwareThatCannotDownload_data()
{
    QTest::addColumn<int>("rc");
    QTest::addColumn<QString>("key");

    QTest::newRow("transfer failed") << -6 << "update.errDownload";
    QTest::newRow("no download function")
        << kInkViewNetUnsupported << "update.errUnsupported";
}

void TestUpdater::aFailedTransferIsToldApartFromAFirmwareThatCannotDownload()
{
    QFETCH(int, rc);
    QFETCH(QString, key);

    answer(QByteArray(), rc);
    updater_->check();

    QCOMPARE(updater_->state(), QStringLiteral("error"));
    QCOMPARE(updater_->errorKey(), key);
    /* The firmware's own wording for it, for the line under the button. */
    QVERIFY(!updater_->errorDetail().isEmpty());
}

/* The check and the install are separate button presses, and Wi-Fi drops
 * between them. */
void TestUpdater::installNeedsTheNetworkOfItsOwn()
{
    answer(release(QStringLiteral("v99.0.0")));
    updater_->check();
    QCOMPARE(updater_->state(), QStringLiteral("available"));

    InkViewStub::networkUp = false;
    updater_->install();

    QCOMPARE(updater_->state(), QStringLiteral("error"));
    QCOMPARE(updater_->errorKey(), QStringLiteral("update.errNoNetwork"));
}

/* The device has no console and the About screen does not scroll, so the tail
 * of the log is the whole diagnosis when an update ends in "error". */
void TestUpdater::theLogIsWhatTheScreenShowsWhenARunFails()
{
    InkViewStub::networkUp = false;
    updater_->check();

    const QString tail = updater_->diagnostics();
    QVERIFY2(tail.contains(QStringLiteral("check: pressed")), qPrintable(tail));
    QVERIFY2(tail.contains(QStringLiteral("state: error")), qPrintable(tail));
    /* Eight lines at most: anything longer runs off the bottom of a screen
     * that cannot scroll. */
    QVERIFY(tail.split(QLatin1Char('\n')).size() <= 8);
}

QTEST_GUILESS_MAIN(TestUpdater)
#include "tst_updater.moc"
