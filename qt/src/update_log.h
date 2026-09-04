#pragma once

#include <QString>

/* Append-and-close trace of the update path, kept because the failure mode we
 * need to see is the process dying mid-call: a buffered log would lose exactly
 * the line that matters. Read back into the About tab so a device without a
 * cable can still say where it stopped. */
void updateLog(const QString &line);
QString updateLogTail(int lines = 12);
