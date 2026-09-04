.pragma library

/* Norwegian (Bokmål) catalog. Two plural forms. */

function plural(n) {
    return n === 1 ? 0 : 1;
}

var strings = {
    "app.title": "Statistikk",

    "nav.overview": "Oversikt",
    "nav.calendar": "Kalender",

    "overview.left": "Ca. {time} igjen",
    "overview.noBook": "Ingen bok åpnet ennå",
    "overview.bookProgress": "Bokens framdrift: {percent} %",
    "overview.allBooks": "ALLE BØKER",
    "overview.booksFinished": "ferdigleste bøker",
    "overview.totalHours": "timer totalt",
    "overview.pagesPerHour": "sider i timen",


    "calendar.dayTitle": "{date}  ·  {time}",
    "calendar.finished": "Ferdiglest",
    "calendar.trackingSince": "Lesedata er registrert siden {date}.",
    "book.finishedOn": "Ferdiglest {date}",
    "calendar.monthSummary": "{n} {days} · {time}",


    "edit.ok": "OK",
    "edit.hoursTitle": "LESETIMER TOTALT",
    "edit.booksTitle": "LESTE BØKER TOTALT",

    "streak.currentCaption": "dager på rad nå",
    "streak.bestCaption": "beste rekke {year}",
    "streak.readDaysCaption": "{n} {days} med lesing i {year}",
    "streak.notRead": "ikke lest",
    "streak.read": "lest",

    "about.section": "OM APPEN",
    "about.streak": "Du har lest {n} {days} på rad!",
    "about.check": "Se etter oppdatering",
    "about.install": "Installer {version}",
    "about.connecting": "Bringing Wi-Fi up…",
    "about.checking": "Spør GitHub om siste utgivelse…",
    "about.uptodate": "Dette er den nyeste versjonen.",
    "about.available": "Versjon {version} er tilgjengelig.",
    "about.downloading": "Laster ned oppdateringen…",
    "about.ready": "Oppdateringen er lastet ned. {app} lukkes og starter av seg selv — hvis ikke, åpne den fra menyen.",
    "about.autostart": "AUTOSTART",
    "about.log": "Siste forsøk:",

    "update.errNoNetwork": "Ingen tilkobling. Slå på wifi og prøv igjen.",
    "update.errDownload": "Nedlastingen mislyktes.",
    "update.errResponse": "GitHub svarte uventet.",
    "update.errNoAsset": "Siste utgivelse inneholder ingen installerbar fil.",
    "update.errUnsupported": "Denne fastvaren kan ikke laste ned oppdateringen.",
    "update.errCorrupt": "Den nedlastede filen er skadet — ingenting ble endret.",
    "update.errHandover": "Appen kunne ikke byttes ut. Den nye versjonen ligger her:",

    "date.months": ["Januar", "Februar", "Mars", "April", "Mai", "Juni", "Juli", "August", "September", "Oktober", "November", "Desember"],
    "date.monthsGen": ["januar", "februar", "mars", "april", "mai", "juni", "juli", "august", "september", "oktober", "november", "desember"],
    "date.weekdays": ["Man", "Tir", "Ons", "Tor", "Fre", "Lør", "Søn"],
    "date.dayMonth": "{d}. {monthGen}",

    "time.hm": "{h} t {m} min",
    "time.m": "{m} min",

    "plural.days": ["dag", "dager"]
};
