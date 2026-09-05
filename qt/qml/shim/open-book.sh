#!/bin/sh
# PocketBook Statistics — open-book shim.
#
# Installed by the app (About screen) into /mnt/ext1/system/bin, and named for
# the reading formats in /mnt/ext1/system/config/extensions.cfg. The firmware
# runs it in place of the reader when a book is opened: it makes sure the stats
# daemon is running, then hands the book to the reader that would have opened
# it anyway.
#
# The book must open whatever happens above the handover, so every step before
# it is optional and failure-tolerant.

SELF='pbstatistics-open.app'
APP='/mnt/ext1/applications/PocketBookStatistics.app'
PIDFILE='/mnt/ext1/system/pocketbook-statistics/statistics.pid'
LOG='/mnt/ext1/system/pocketbook-statistics/open.log'
SYS_EXT='/ebrmain/config/extensions.cfg'
START_DELAY=45

log() {
    echo "$(date '+%m-%d %H:%M:%S') $*" >>"$LOG" 2>/dev/null
}

# The pidfile is read here rather than by the app, because asking the app costs
# a full start of a binary linked against the firmware's Qt — the loader maps
# QtGui/QtQml/QtQuick before main() runs, even in --daemon mode, and the book
# opens that much later. Nearly every open finds a live daemon and starts
# nothing at all.
# A live pid is not our pid. The pidfile survives a reboot and the number in it
# is then regularly some process of the firmware's, so ask what the process
# actually is before believing the daemon runs.
DPID=$(cat "$PIDFILE" 2>/dev/null)
DCMD=''
[ -n "$DPID" ] && [ -r "/proc/$DPID/cmdline" ] \
    && DCMD=$(tr -d '\000' < "/proc/$DPID/cmdline" 2>/dev/null)
# Pure shell on purpose: no grep on a file full of NUL bytes, whose behaviour
# differs between busybox and GNU. Dropping the separators glues the arguments
# together, which the pattern allows for.
case "$DCMD" in
    *PocketBookStatistics*--daemon*) DAEMON_UP=1 ;;
    *)                               DAEMON_UP=0 ;;
esac

if [ "$DAEMON_UP" = 1 ]; then
    log "daemon: already running ($DPID)"
elif [ -x "$APP" ]; then
    # Wait first: the daemon is not needed for another poll interval, and the
    # reader is what the user is waiting for. The two must not compete for CPU
    # and flash while the book is opening.
    if command -v setsid >/dev/null 2>&1; then
        (sleep "$START_DELAY"; setsid "$APP" --daemon >/dev/null 2>&1) &
    else
        (sleep "$START_DELAY"; "$APP" --daemon >/dev/null 2>&1) &
    fi
    log "daemon: starting in ${START_DELAY}s"
else
    log "daemon: $APP missing"
fi

# Which reader would have opened this? The firmware's own table lists the
# applications per extension, and we put ourselves at the front of that list on
# install — so the answer is the next name along.
book="$1"
ext=$(printf '%s' "${book##*.}" | tr 'A-Z' 'a-z')
reader=''

line=$(grep -i "^${ext}:" "$SYS_EXT" 2>/dev/null | head -n 1)
if [ -n "$line" ]; then
    for app in $(printf '%s' "$line" | cut -d: -f4 | tr ',' ' '); do
        [ "$app" = "$SELF" ] && continue
        for dir in /ebrmain/bin /mnt/ext1/system/bin; do
            if [ -x "$dir/$app" ]; then
                reader="$dir/$app"
                break
            fi
        done
        [ -n "$reader" ] && break
    done
fi

# Nothing usable in the table (or no table): fall back to the readers a PB629
# ships, in the order its handlers.cfg names them.
if [ -z "$reader" ]; then
    for cand in \
        /ebrmain/bin/eink-reader.app \
        /ebrmain/bin/eink-reader_with_blink.app \
        /ebrmain/bin/eink-reader_with_rmsdk.app \
        /ebrmain/bin/reader.app
    do
        if [ -x "$cand" ]; then
            reader="$cand"
            break
        fi
    done
fi

if [ -n "$reader" ]; then
    log "handover: $reader"
    exec "$reader" "$@"
fi

log "handover: no reader found for .$ext"
exit 1
