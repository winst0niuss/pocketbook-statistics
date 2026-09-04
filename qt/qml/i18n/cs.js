.pragma library

/* Czech catalog. Three plural forms: 1 kniha, 2 knihy, 5 knih. */

function plural(n) {
    if (n === 1)
        return 0;
    if (n >= 2 && n <= 4)
        return 1;
    return 2;
}

var strings = {
    "app.title": "Statistika",

    "nav.overview": "Přehled",
    "nav.calendar": "Kalendář",

    "overview.left": "Zbývá asi {time}",
    "overview.noBook": "Zatím nebyla otevřena kniha",
    "overview.bookProgress": "Postup knihy: {percent} %",
    "overview.allBooks": "VŠECHNY KNIHY",
    "overview.booksFinished": "dočtené knihy",
    "overview.totalHours": "hodin celkem",
    "overview.pagesPerHour": "stran za hodinu",


    "calendar.dayTitle": "{date}  ·  {time}",
    "calendar.finished": "Dočteno",
    "calendar.trackingSince": "Údaje o čtení se zaznamenávají od {date}.",
    "book.finishedOn": "Dočteno {date}",
    "calendar.monthSummary": "{n} {days} · {time}",


    "edit.ok": "OK",
    "edit.hoursTitle": "CELKEM HODIN ČTENÍ",
    "edit.booksTitle": "CELKEM PŘEČTENÝCH KNIH",

    "streak.currentCaption": "dní v řadě nyní",
    "streak.bestCaption": "nejlepší série v {year}",
    "streak.readDaysCaption": "{n} {days} čtení v {year}",
    "streak.notRead": "bez čtení",
    "streak.read": "čtení",

    "about.section": "O APLIKACI",
    "about.streak": "Čteš už {n} {days} v řadě!",
    "about.check": "Zkontrolovat aktualizaci",
    "about.install": "Nainstalovat {version}",
    "about.connecting": "Bringing Wi-Fi up…",
    "about.checking": "Ptám se GitHubu na nejnovější vydání…",
    "about.uptodate": "Toto je nejnovější verze.",
    "about.available": "K dispozici je verze {version}.",
    "about.downloading": "Stahuji aktualizaci…",
    "about.ready": "Aktualizace stažena. {app} se zavře a sám spustí znovu — pokud ne, otevřete jej z nabídky aplikací.",
    "about.autostart": "AUTOMATICKÉ SPUŠTĚNÍ",
    "about.log": "Poslední pokus:",

    "update.errNoNetwork": "Bez připojení. Zapněte Wi-Fi a zkuste to znovu.",
    "update.errDownload": "Stažení se nezdařilo.",
    "update.errResponse": "GitHub odpověděl neočekávaně.",
    "update.errNoAsset": "Nejnovější vydání neobsahuje hotové sestavení.",
    "update.errUnsupported": "Tento firmware neumožňuje aktualizaci stáhnout.",
    "update.errCorrupt": "Stažený soubor je poškozený — nic se nezměnilo.",
    "update.errHandover": "Aplikaci se nepodařilo vyměnit. Nová verze je zde:",

    "date.months": ["Leden", "Únor", "Březen", "Duben", "Květen", "Červen", "Červenec", "Srpen", "Září", "Říjen", "Listopad", "Prosinec"],
    "date.monthsGen": ["ledna", "února", "března", "dubna", "května", "června", "července", "srpna", "září", "října", "listopadu", "prosince"],
    "date.weekdays": ["Po", "Út", "St", "Čt", "Pá", "So", "Ne"],
    "date.dayMonth": "{d}. {monthGen}",

    "time.hm": "{h} h {m} min",
    "time.m": "{m} min",

    "plural.days": ["den", "dny", "dní"]
};
