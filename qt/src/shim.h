#pragma once

#include <QObject>

/* The open-book shim: a small shell script in /mnt/ext1/system/bin that the
 * firmware runs instead of the reader, so the stats daemon is alive from the
 * moment a book is opened — including after a reboot, which is the one case
 * the app cannot cover on its own (nothing of ours starts at boot).
 *
 * Installing means two writes to the user partition, both reversible and both
 * backed up: the script itself, and our name at the front of the application
 * list for each reading format in system/config/extensions.cfg. */
class Shim : public QObject {
    Q_OBJECT

public:
    explicit Shim(QObject *parent = nullptr);

    /* True when the script is in place and at least one format names it. */
    Q_INVOKABLE bool installed() const;
    /* Rewrites the installed script if the app now ships a different one.
     * Called at startup: an app update otherwise leaves the old script in
     * place forever, since nothing else ever touches it. */
    void refresh();
    Q_INVOKABLE bool install();
    Q_INVOKABLE bool remove();
};
