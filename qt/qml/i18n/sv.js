.pragma library

/* Swedish catalog. Two plural forms. */

function plural(n) {
    return n === 1 ? 0 : 1;
}

var strings = {
    "app.title": "Statistik",

    "nav.overview": "Översikt",
    "nav.calendar": "Kalender",

    "overview.left": "Ca {time} kvar",
    "overview.noBook": "Ingen bok öppnad ännu",
    "overview.bookProgress": "Bokens framsteg: {percent} %",
    "overview.allBooks": "ALLA BÖCKER",
    "overview.booksFinished": "utlästa böcker",
    "overview.totalHours": "timmar totalt",
    "overview.pagesPerHour": "sidor per timme",


    "calendar.dayTitle": "{date}  ·  {time}",
    "calendar.finished": "Utläst",
    "calendar.trackingSince": "Läsdata har registrerats sedan {date}.",
    "book.finishedOn": "Utläst {date}",
    "calendar.monthSummary": "{n} {days} · {time}",


    "edit.ok": "OK",
    "edit.hoursTitle": "LÄSTIMMAR TOTALT",
    "edit.booksTitle": "LÄSTA BÖCKER TOTALT",

    "streak.currentCaption": "dagar i rad nu",
    "streak.bestCaption": "bästa svit {year}",
    "streak.readDaysCaption": "{n} {days} med läsning {year}",
    "streak.notRead": "inte läst",
    "streak.read": "läst",

    "about.section": "OM APPEN",
    "about.streak": "Du har läst {n} {days} i rad!",
    "about.check": "Sök efter uppdatering",
    "about.install": "Installera {version}",
    "about.connecting": "Bringing Wi-Fi up…",
    "about.checking": "Frågar GitHub efter senaste versionen…",
    "about.uptodate": "Detta är den senaste versionen.",
    "about.available": "Version {version} finns tillgänglig.",
    "about.downloading": "Hämtar uppdateringen…",
    "about.ready": "Uppdateringen är hämtad. {app} stängs och startar om av sig själv — annars öppnar du den från menyn.",
    "about.autostart": "AUTOSTART",
    "about.log": "Senaste försöket:",

    "update.errNoNetwork": "Ingen anslutning. Slå på wifi och försök igen.",
    "update.errDownload": "Hämtningen misslyckades.",
    "update.errResponse": "GitHub svarade oväntat.",
    "update.errNoAsset": "Den senaste versionen innehåller ingen installerbar fil.",
    "update.errUnsupported": "Den här firmwaren kan inte hämta uppdateringen.",
    "update.errCorrupt": "Den hämtade filen är skadad — inget har ändrats.",
    "update.errHandover": "Det gick inte att byta ut appen. Den nya versionen finns här:",

    "date.months": ["Januari", "Februari", "Mars", "April", "Maj", "Juni", "Juli", "Augusti", "September", "Oktober", "November", "December"],
    "date.monthsGen": ["januari", "februari", "mars", "april", "maj", "juni", "juli", "augusti", "september", "oktober", "november", "december"],
    "date.weekdays": ["Mån", "Tis", "Ons", "Tor", "Fre", "Lör", "Sön"],
    "date.dayMonth": "{d} {monthGen}",

    "time.hm": "{h} tim {m} min",
    "time.m": "{m} min",

    "plural.days": ["dag", "dagar"]
};
