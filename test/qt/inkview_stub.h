#pragma once

#include <QByteArray>
#include <QList>
#include <QString>

/* Stands in for inkview_bridge.cpp, the one TU that talks to the firmware.
 * Nothing in it can run off the device — the functions it calls are resolved
 * with dlsym out of the reader's own libinkview.so — so the host tests link
 * this instead and script it: what language the device claims to be in, whether
 * Wi-Fi comes up, and what the next download lands in the file. */
namespace InkViewStub {

struct Download {
    int rc = 0;          /* 0, or a negative NET_E* code */
    QByteArray body;     /* written to the destination when rc == 0 */
};

/* Scripted answers, consumed in order; an empty queue is an empty 200. */
extern QList<Download> downloads;
extern QList<QString> requestedUrls;
extern QList<int> requestedTimeouts;
extern QList<QString> openedBooks;
extern QString lang;
extern bool networkUp;
extern bool openBookSupported;

void reset();

} // namespace InkViewStub
