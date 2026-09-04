.pragma library

/* Estonian catalog. Two plural forms. */

function plural(n) {
    return n === 1 ? 0 : 1;
}

var strings = {
    "app.title": "Statistika",

    "nav.overview": "Ülevaade",
    "nav.calendar": "Kalender",

    "overview.left": "Jäänud u {time}",
    "overview.noBook": "Ühtegi raamatut pole veel avatud",
    "overview.bookProgress": "Raamatu edenemine: {percent} %",
    "overview.allBooks": "KÕIK RAAMATUD",
    "overview.booksFinished": "läbi loetud raamatud",
    "overview.totalHours": "tunde kokku",
    "overview.pagesPerHour": "lehekülge tunnis",


    "calendar.dayTitle": "{date}  ·  {time}",
    "calendar.finished": "Läbi loetud",
    "calendar.trackingSince": "Lugemisandmeid on salvestatud alates {date}.",
    "book.finishedOn": "Läbi loetud {date}",
    "calendar.monthSummary": "{n} {days} · {time}",


    "streak.currentCaption": "päeva järjest",
    "streak.bestCaption": "parim seeria {year}",
    "streak.readDaysCaption": "{n} {days} lugemist aastal {year}",
    "streak.notRead": "lugemata",
    "streak.read": "loetud",

    "about.section": "RAKENDUSEST",
    "about.streak": "Oled lugenud {n} {days} järjest!",
    "about.check": "Otsi uuendust",
    "about.install": "Paigalda {version}",
    "about.connecting": "Bringing Wi-Fi up…",
    "about.checking": "Küsin GitHubilt viimast väljalaset…",
    "about.uptodate": "See on kõige uuem versioon.",
    "about.available": "Saadaval on versioon {version}.",
    "about.downloading": "Laadin uuendust…",
    "about.ready": "Uuendus alla laaditud. {app} sulgub ja käivitub ise uuesti — kui mitte, ava see rakenduste menüüst.",
    "about.autostart": "AUTOMAATNE KÄIVITUS",
    "about.log": "Viimane katse:",

    "update.errNoNetwork": "Ühendus puudub. Lülita WiFi sisse ja proovi uuesti.",
    "update.errDownload": "Allalaadimine ebaõnnestus.",
    "update.errResponse": "GitHub vastas ootamatult.",
    "update.errNoAsset": "Viimane väljalase ei sisalda paigaldatavat faili.",
    "update.errUnsupported": "See püsivara ei võimalda uuendust alla laadida.",
    "update.errCorrupt": "Allalaaditud fail on rikutud — midagi ei muudetud.",
    "update.errHandover": "Rakendust ei õnnestunud vahetada. Uus versioon on siin:",

    "date.months": ["Jaanuar", "Veebruar", "Märts", "Aprill", "Mai", "Juuni", "Juuli", "August", "September", "Oktoober", "November", "Detsember"],
    "date.monthsGen": ["jaanuar", "veebruar", "märts", "aprill", "mai", "juuni", "juuli", "august", "september", "oktoober", "november", "detsember"],
    "date.weekdays": ["E", "T", "K", "N", "R", "L", "P"],
    "date.dayMonth": "{d}. {monthGen}",

    "time.hm": "{h} t {m} min",
    "time.m": "{m} min",

    "plural.days": ["päev", "päeva"]
};
