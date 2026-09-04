.pragma library

/* Slovenian catalog. Four plural forms: 1 knjiga, 2 knjigi, 3 knjige, 5 knjig. */

function plural(n) {
    var m100 = n % 100;
    if (m100 === 1)
        return 0;
    if (m100 === 2)
        return 1;
    if (m100 === 3 || m100 === 4)
        return 2;
    return 3;
}

var strings = {
    "app.title": "Statistika",

    "nav.overview": "Pregled",
    "nav.calendar": "Koledar",

    "overview.left": "Ostalo pribl. {time}",
    "overview.noBook": "Nobena knjiga še ni odprta",
    "overview.bookProgress": "Napredek knjige: {percent} %",
    "overview.allBooks": "VSE KNJIGE",
    "overview.booksFinished": "prebrane knjige",
    "overview.totalHours": "ur skupaj",
    "overview.pagesPerHour": "strani na uro",


    "calendar.dayTitle": "{date}  ·  {time}",
    "calendar.finished": "Prebrana",
    "calendar.trackingSince": "Podatki o branju se beležijo od {date}.",
    "book.finishedOn": "Prebrana {date}",
    "calendar.monthSummary": "{n} {days} · {time}",


    "edit.ok": "OK",
    "edit.hoursTitle": "SKUPAJ UR BRANJA",
    "edit.booksTitle": "SKUPAJ PREBRANIH KNJIG",

    "streak.currentCaption": "dni zapored zdaj",
    "streak.bestCaption": "najboljši niz v {year}",
    "streak.readDaysCaption": "{n} {days} branja v {year}",
    "streak.notRead": "brez branja",
    "streak.read": "branje",

    "about.section": "O APLIKACIJI",
    "about.streak": "Bereš že {n} {days} zapored!",
    "about.check": "Preveri posodobitev",
    "about.install": "Namesti {version}",
    "about.connecting": "Bringing Wi-Fi up…",
    "about.checking": "Sprašujem GitHub po zadnji izdaji…",
    "about.uptodate": "To je najnovejša različica.",
    "about.available": "Na voljo je različica {version}.",
    "about.downloading": "Prenašam posodobitev…",
    "about.ready": "Posodobitev je prenesena. {app} se bo zaprl in sam znova zagnal — če se ne, ga odprite iz menija aplikacij.",
    "about.autostart": "SAMODEJNI ZAGON",
    "about.log": "Zadnji poskus:",

    "update.errNoNetwork": "Ni povezave. Vklopite Wi-Fi in poskusite znova.",
    "update.errDownload": "Prenos ni uspel.",
    "update.errResponse": "GitHub je odgovoril nepričakovano.",
    "update.errNoAsset": "Zadnja izdaja nima namestitvene datoteke.",
    "update.errUnsupported": "Ta strojna programska oprema ne more prenesti posodobitve.",
    "update.errCorrupt": "Prenesena datoteka je poškodovana — nič ni bilo spremenjeno.",
    "update.errHandover": "Aplikacije ni bilo mogoče zamenjati. Nova različica je tukaj:",

    "date.months": ["Januar", "Februar", "Marec", "April", "Maj", "Junij", "Julij", "Avgust", "September", "Oktober", "November", "December"],
    "date.monthsGen": ["januarja", "februarja", "marca", "aprila", "maja", "junija", "julija", "avgusta", "septembra", "oktobra", "novembra", "decembra"],
    "date.weekdays": ["Pon", "Tor", "Sre", "Čet", "Pet", "Sob", "Ned"],
    "date.dayMonth": "{d}. {monthGen}",

    "time.hm": "{h} h {m} min",
    "time.m": "{m} min",

    "plural.days": ["dan", "dneva", "dnevi", "dni"]
};
