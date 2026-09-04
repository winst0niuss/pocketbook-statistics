.pragma library

/* Latvian catalog. Three plural forms, the third for zero. */

function plural(n) {
    if (n % 10 === 1 && n % 100 !== 11)
        return 0;
    if (n !== 0)
        return 1;
    return 2;
}

var strings = {
    "app.title": "Statistika",

    "nav.overview": "Pārskats",
    "nav.calendar": "Kalendārs",

    "overview.left": "Atlicis apm. {time}",
    "overview.noBook": "Neviena grāmata vēl nav atvērta",
    "overview.bookProgress": "Grāmatas progress: {percent} %",
    "overview.allBooks": "VISAS GRĀMATAS",
    "overview.booksFinished": "izlasītās grāmatas",
    "overview.totalHours": "stundas kopā",
    "overview.pagesPerHour": "lappuses stundā",


    "calendar.dayTitle": "{date}  ·  {time}",
    "calendar.finished": "Izlasīta",
    "calendar.trackingSince": "Lasīšanas dati tiek uzkrāti kopš {date}.",
    "book.finishedOn": "Izlasīta {date}",
    "calendar.monthSummary": "{n} {days} · {time}",


    "streak.currentCaption": "dienas pēc kārtas",
    "streak.bestCaption": "labākā sērija {year}",
    "streak.readDaysCaption": "{n} {days} ar lasīšanu {year}",
    "streak.notRead": "nav lasīts",
    "streak.read": "lasīts",

    "about.section": "PAR LIETOTNI",
    "about.streak": "Tu lasi jau {n} {days} pēc kārtas!",
    "about.check": "Meklēt atjauninājumu",
    "about.install": "Instalēt {version}",
    "about.connecting": "Bringing Wi-Fi up…",
    "about.checking": "Vaicāju GitHub par jaunāko laidienu…",
    "about.uptodate": "Šī ir jaunākā versija.",
    "about.available": "Pieejama versija {version}.",
    "about.downloading": "Lejupielādēju atjauninājumu…",
    "about.ready": "Atjauninājums lejupielādēts. {app} aizvērsies un pats startēs no jauna — ja nē, atveriet to no lietotņu izvēlnes.",
    "about.autostart": "AUTOMĀTISKĀ PALAIŠANA",
    "about.log": "Pēdējais mēģinājums:",

    "update.errNoNetwork": "Nav savienojuma. Ieslēdziet Wi-Fi un mēģiniet vēlreiz.",
    "update.errDownload": "Lejupielāde neizdevās.",
    "update.errResponse": "GitHub atbildēja negaidīti.",
    "update.errNoAsset": "Jaunākajā laidienā nav instalējamas datnes.",
    "update.errUnsupported": "Šī programmaparatūra nevar lejupielādēt atjauninājumu.",
    "update.errCorrupt": "Lejupielādētā datne ir bojāta — nekas netika mainīts.",
    "update.errHandover": "Lietotni neizdevās nomainīt. Jaunā versija ir šeit:",

    "date.months": ["Janvāris", "Februāris", "Marts", "Aprīlis", "Maijs", "Jūnijs", "Jūlijs", "Augusts", "Septembris", "Oktobris", "Novembris", "Decembris"],
    "date.monthsGen": ["janvāra", "februāra", "marta", "aprīļa", "maija", "jūnija", "jūlija", "augusta", "septembra", "oktobra", "novembra", "decembra"],
    "date.weekdays": ["Pr", "Ot", "Tr", "Ce", "Pk", "Se", "Sv"],
    "date.dayMonth": "{d}. {monthGen}",

    "time.hm": "{h} st {m} min",
    "time.m": "{m} min",

    "plural.days": ["diena", "dienas", "dienu"]
};
