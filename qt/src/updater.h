#pragma once

#include <QObject>
#include <QString>

/* Self-update against the project's GitHub releases.
 *
 * The only part of the app that touches the network, and only when the user
 * presses a button: nothing here runs on its own. The transfers themselves are
 * the firmware's (see inkview_bridge), so no TLS or certificate handling of
 * our own.
 *
 * Both calls block until the firmware is done — that is deliberate. InkView is
 * not safe to drive from a second thread, and an e-ink screen has no animation
 * to keep alive; the state is published and the scene repainted before every
 * blocking step so the user sees what is happening. */
class Updater : public QObject {
    Q_OBJECT

    /* "idle" | "checking" | "uptodate" | "available" | "downloading"
     * | "ready" | "error" */
    Q_PROPERTY(QString state READ state NOTIFY changed)
    Q_PROPERTY(QString currentVersion READ currentVersion CONSTANT)
    Q_PROPERTY(QString latestVersion READ latestVersion NOTIFY changed)
    /* i18n key for the failure, plus the firmware's own wording for it. */
    Q_PROPERTY(QString errorKey READ errorKey NOTIFY changed)
    Q_PROPERTY(QString errorDetail READ errorDetail NOTIFY changed)
    /* Tail of the update log — the only way to see how far a run got when the
     * firmware takes the process down with it. */
    Q_PROPERTY(QString diagnostics READ diagnostics NOTIFY changed)

public:
    explicit Updater(QObject *parent = nullptr);

    QString state() const { return state_; }
    QString currentVersion() const;
    QString latestVersion() const { return latestVersion_; }
    QString errorKey() const { return errorKey_; }
    QString errorDetail() const { return errorDetail_; }
    QString diagnostics() const;

    /* Asks GitHub for the latest release and compares it with our version. */
    Q_INVOKABLE void check();
    /* Downloads that release and stages it; the app must quit afterwards for
     * the handover script to swap the binary. */
    Q_INVOKABLE void install();

signals:
    void changed();

private:
    void publish(const QString &state);
    void fail(const QString &key, const QString &detail = QString());
    bool download(const QString &url, const QString &dest,
                  int timeoutSeconds, bool retry);
    bool unpack(const QString &zip, const QString &dest);
    bool launchHandover(const QString &staged);

    QString state_ = QStringLiteral("idle");
    QString latestVersion_;
    QString assetUrl_;
    qint64 assetSize_ = 0;
    QString errorKey_;
    QString errorDetail_;
};
