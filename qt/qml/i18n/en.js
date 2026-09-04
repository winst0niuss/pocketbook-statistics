.pragma library

/* English catalog — also the fallback for every key a catalog is missing
 * and for device languages we have no catalog for. */

function plural(n) {
    return n === 1 ? 0 : 1;
}

var strings = {
    "app.title": "Statistics",

    "nav.overview": "Overview",
    "nav.calendar": "Calendar",

    "overview.left": "About {time}",
    "overview.noBook": "No book opened yet",
    "overview.bookProgress": "Book progress: {percent} %",
    "overview.currentBook": "CURRENT BOOK",
    "overview.today": "TODAY",
    "overview.minutesToday": "minutes read",
    "overview.allBooks": "ALL BOOKS",
    "overview.booksFinished": "books finished",
    "overview.totalHours": "total hours",
    "overview.pagesPerHour": "pages per hour",


    "calendar.dayTitle": "{date}  ·  {time}",
    "calendar.finished": "Finished",
    "calendar.trackingSince": "Reading data has been recorded since {date}.",
    "book.finishedOn": "Finished {date}",
    "calendar.monthSummary": "{n} {days} · {time}",


    "streak.currentCaption": "days in a row now",
    "streak.bestCaption": "best streak in {year}",
    "streak.readDaysCaption": "{n} {days} of reading in {year}",
    "streak.notRead": "not read",
    "streak.read": "read",

    "about.section": "ABOUT",
    "about.streak": "You've read {n} {days} in a row!",
    "about.check": "Check for update",
    "about.install": "Install {version}",
    "about.connecting": "Bringing Wi-Fi up…",
    "about.checking": "Asking GitHub for the latest release\u2026",
    "about.uptodate": "This is the latest release.",
    "about.available": "Version {version} is available.",
    "about.downloading": "Downloading the update\u2026",
    "about.ready": "Update downloaded. {app} closes now and starts again on its own \u2014 if it doesn't, open it from the launcher.",

    "about.autostart": "AUTOSTART",
    "about.shim": "Start statistics when a book opens",
    "about.shimHint": "Opening an EPUB, FB2 or PDF starts tracking.",
    "about.log": "Last attempt:",

    "update.errNoNetwork": "No connection. Turn on Wi-Fi and try again.",
    "update.errDownload": "Download failed.",
    "update.errResponse": "GitHub sent an unexpected answer.",
    "update.errNoAsset": "The latest release ships no installable build.",
    "update.errUnsupported": "This firmware offers no way to download the update.",
    "update.errCorrupt": "The downloaded file is damaged \u2014 nothing was changed.",
    "update.errHandover": "The new version could not be swapped in. It is stored here:",

    "date.months": ["January", "February", "March", "April", "May", "June",
                    "July", "August", "September", "October", "November", "December"],
    "date.monthsGen": ["January", "February", "March", "April", "May", "June",
                       "July", "August", "September", "October", "November", "December"],
    "date.weekdays": ["Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"],
    "date.dayMonth": "{month} {d}",

    "time.hm": "{h}h {m}m",
    "time.m": "{m} min",

    "plural.days": ["day", "days"],
    "plural.minutesToday": ["minute read", "minutes read"],
    "plural.booksFinished": ["book finished", "books finished"],
    "plural.totalHours": ["total hour", "total hours"]
};
