.pragma library

/* Croatian catalog. Three plural forms: 1 knjiga, 2 knjige, 5 knjiga. */

function plural(n) {
    var m10 = n % 10, m100 = n % 100;
    if (m10 === 1 && m100 !== 11)
        return 0;
    if (m10 >= 2 && m10 <= 4 && (m100 < 12 || m100 > 14))
        return 1;
    return 2;
}

var strings = {
    "app.title": "Statistika",

    "nav.overview": "Pregled",
    "nav.calendar": "Kalendar",

    "overview.left": "Preostalo oko {time}",
    "overview.noBook": "Nijedna knjiga još nije otvorena",
    "overview.bookProgress": "Napredak knjige: {percent} %",
    "overview.allBooks": "SVE KNJIGE",
    "overview.booksFinished": "pročitane knjige",
    "overview.totalHours": "sati ukupno",
    "overview.pagesPerHour": "stranica na sat",


    "calendar.dayTitle": "{date}  ·  {time}",
    "calendar.finished": "Pročitana",
    "calendar.trackingSince": "Podaci o čitanju bilježe se od {date}.",
    "book.finishedOn": "Pročitana {date}",
    "calendar.monthSummary": "{n} {days} · {time}",


    "edit.ok": "OK",
    "edit.hoursTitle": "UKUPNO SATI ČITANJA",
    "edit.booksTitle": "UKUPNO PROČITANIH KNJIGA",

    "streak.currentCaption": "dana zaredom sada",
    "streak.bestCaption": "najbolja serija {year}",
    "streak.readDaysCaption": "{n} {days} čitanja u {year}",
    "streak.notRead": "bez čitanja",
    "streak.read": "čitanje",

    "about.section": "O APLIKACIJI",
    "about.streak": "Čitaš već {n} {days} zaredom!",
    "about.check": "Provjeri ažuriranje",
    "about.install": "Instaliraj {version}",
    "about.connecting": "Bringing Wi-Fi up…",
    "about.checking": "Pitam GitHub za najnovije izdanje…",
    "about.uptodate": "Ovo je najnovija verzija.",
    "about.available": "Dostupna je verzija {version}.",
    "about.downloading": "Preuzimam ažuriranje…",
    "about.ready": "Ažuriranje je preuzeto. {app} će se zatvoriti i sam ponovno pokrenuti — ako ne, otvorite ga iz izbornika aplikacija.",
    "about.autostart": "AUTOMATSKO POKRETANJE",
    "about.log": "Posljednji pokušaj:",

    "update.errNoNetwork": "Nema veze. Uključite Wi-Fi i pokušajte ponovno.",
    "update.errDownload": "Preuzimanje nije uspjelo.",
    "update.errResponse": "GitHub je odgovorio neočekivano.",
    "update.errNoAsset": "Najnovije izdanje nema datoteku za instalaciju.",
    "update.errUnsupported": "Ovaj firmver ne može preuzeti ažuriranje.",
    "update.errCorrupt": "Preuzeta datoteka je oštećena — ništa nije promijenjeno.",
    "update.errHandover": "Aplikaciju nije bilo moguće zamijeniti. Nova verzija je ovdje:",

    "date.months": ["Siječanj", "Veljača", "Ožujak", "Travanj", "Svibanj", "Lipanj", "Srpanj", "Kolovoz", "Rujan", "Listopad", "Studeni", "Prosinac"],
    "date.monthsGen": ["siječnja", "veljače", "ožujka", "travnja", "svibnja", "lipnja", "srpnja", "kolovoza", "rujna", "listopada", "studenoga", "prosinca"],
    "date.weekdays": ["Pon", "Uto", "Sri", "Čet", "Pet", "Sub", "Ned"],
    "date.dayMonth": "{d}. {monthGen}",

    "time.hm": "{h} h {m} min",
    "time.m": "{m} min",

    "plural.days": ["dan", "dana", "dana"]
};
