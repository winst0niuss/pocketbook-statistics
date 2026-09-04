.pragma library

/* Lithuanian catalog. Three plural forms: 1 knyga, 2 knygos, 10 knygų. */

function plural(n) {
    var m10 = n % 10, m100 = n % 100;
    if (m10 === 1 && (m100 < 11 || m100 > 19))
        return 0;
    if (m10 >= 2 && m10 <= 9 && (m100 < 11 || m100 > 19))
        return 1;
    return 2;
}

var strings = {
    "app.title": "Statistika",

    "nav.overview": "Apžvalga",
    "nav.calendar": "Kalendorius",

    "overview.left": "Liko apie {time}",
    "overview.noBook": "Dar neatverstos jokios knygos",
    "overview.bookProgress": "Knygos progresas: {percent} %",
    "overview.allBooks": "VISOS KNYGOS",
    "overview.booksFinished": "perskaitytos knygos",
    "overview.totalHours": "iš viso valandų",
    "overview.pagesPerHour": "puslapių per valandą",


    "calendar.dayTitle": "{date}  ·  {time}",
    "calendar.finished": "Perskaityta",
    "calendar.trackingSince": "Skaitymo duomenys renkami nuo {date}.",
    "book.finishedOn": "Perskaityta {date}",
    "calendar.monthSummary": "{n} {days} · {time}",


    "edit.reset": "Atstatyti",

    "streak.currentCaption": "dienos iš eilės",
    "streak.bestCaption": "geriausia serija {year}",
    "streak.readDaysCaption": "{n} {days} su skaitymu {year}",
    "streak.notRead": "neskaityta",
    "streak.read": "skaityta",

    "about.section": "APIE PROGRAMĖLĘ",
    "about.streak": "Skaitai jau {n} {days} iš eilės!",
    "about.check": "Tikrinti atnaujinimą",
    "about.install": "Įdiegti {version}",
    "about.connecting": "Bringing Wi-Fi up…",
    "about.checking": "Klausiu GitHub apie naujausią laidą…",
    "about.uptodate": "Tai naujausia versija.",
    "about.available": "Galima versija {version}.",
    "about.downloading": "Atsiunčiu atnaujinimą…",
    "about.ready": "Atnaujinimas atsiųstas. {app} užsidarys ir pats pasileis iš naujo — jei ne, atverkite jį iš programų meniu.",
    "about.autostart": "AUTOMATINIS PALEIDIMAS",
    "about.log": "Paskutinis bandymas:",

    "update.errNoNetwork": "Nėra ryšio. Įjunkite Wi-Fi ir bandykite dar kartą.",
    "update.errDownload": "Atsiuntimas nepavyko.",
    "update.errResponse": "GitHub atsakė netikėtai.",
    "update.errNoAsset": "Naujausioje laidoje nėra įdiegiamos rinkmenos.",
    "update.errUnsupported": "Ši aparatinė programinė įranga negali atsiųsti atnaujinimo.",
    "update.errCorrupt": "Atsiųsta rinkmena sugadinta — niekas nepakeista.",
    "update.errHandover": "Programos pakeisti nepavyko. Nauja versija yra čia:",

    "date.months": ["Sausis", "Vasaris", "Kovas", "Balandis", "Gegužė", "Birželis", "Liepa", "Rugpjūtis", "Rugsėjis", "Spalis", "Lapkritis", "Gruodis"],
    "date.monthsGen": ["sausio", "vasario", "kovo", "balandžio", "gegužės", "birželio", "liepos", "rugpjūčio", "rugsėjo", "spalio", "lapkričio", "gruodžio"],
    "date.weekdays": ["Pr", "An", "Tr", "Kt", "Pn", "Št", "Sk"],
    "date.dayMonth": "{monthGen} {d} d.",

    "time.hm": "{h} val {m} min",
    "time.m": "{m} min",

    "plural.days": ["diena", "dienos", "dienų"]
};
