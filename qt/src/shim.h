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

    /* True when the script is in place and *every* format this device can open
     * names it. Anything less has to read as off, so pressing the switch
     * finishes the job instead of appearing to be done already. */
    Q_INVOKABLE bool installed() const;
    /* Brings an installed shim up to date: the script if the app now ships a
     * different one, and the extensions.cfg entries if this build reads more
     * formats than the one that installed it. Called at startup — nothing else
     * ever touches either, so a fix to them would otherwise never reach the
     * reader. Does nothing where the shim is not installed. */
    void refresh();
    /* Installs the shim the first time the app runs, because a reading tracker
     * that only counts while it is open is not one. Once ever: a marker file
     * records that the decision has been made, so turning it off again holds. */
    void enableByDefault();
    Q_INVOKABLE bool install();
    Q_INVOKABLE bool remove();
};
