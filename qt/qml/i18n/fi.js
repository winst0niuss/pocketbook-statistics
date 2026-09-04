.pragma library

/* Finnish catalog. Two plural forms. */

function plural(n) {
    return n === 1 ? 0 : 1;
}

var strings = {
    "app.title": "Tilastot",

    "nav.overview": "Yleiskuva",
    "nav.calendar": "Kalenteri",

    "overview.left": "Jäljellä n. {time}",
    "overview.noBook": "Yhtään kirjaa ei ole vielä avattu",
    "overview.bookProgress": "Kirjan edistyminen: {percent} %",
    "overview.allBooks": "KAIKKI KIRJAT",
    "overview.booksFinished": "loppuun luetut kirjat",
    "overview.totalHours": "tunteja yhteensä",
    "overview.pagesPerHour": "sivua tunnissa",


    "calendar.dayTitle": "{date}  ·  {time}",
    "calendar.finished": "Luettu loppuun",
    "calendar.trackingSince": "Lukutietoja on kerätty {date} alkaen.",
    "book.finishedOn": "Luettu loppuun {date}",
    "calendar.monthSummary": "{n} {days} · {time}",


    "edit.ok": "OK",
    "edit.hoursTitle": "LUKUTUNTEJA YHTEENSÄ",
    "edit.booksTitle": "LUETTUJA KIRJOJA YHTEENSÄ",

    "streak.currentCaption": "päivää putkeen nyt",
    "streak.bestCaption": "paras putki {year}",
    "streak.readDaysCaption": "{n} {days} lukemista vuonna {year}",
    "streak.notRead": "ei luettu",
    "streak.read": "luettu",

    "about.section": "TIETOJA",
    "about.streak": "Olet lukenut {n} {days} putkeen!",
    "about.check": "Tarkista päivitys",
    "about.install": "Asenna {version}",
    "about.connecting": "Bringing Wi-Fi up…",
    "about.checking": "Kysytään GitHubilta uusinta julkaisua…",
    "about.uptodate": "Tämä on uusin versio.",
    "about.available": "Versio {version} on saatavilla.",
    "about.downloading": "Ladataan päivitystä…",
    "about.ready": "Päivitys ladattu. {app} sulkeutuu ja käynnistyy itsestään uudelleen — jos ei, avaa se sovellusvalikosta.",
    "about.autostart": "AUTOMAATTINEN KÄYNNISTYS",
    "about.log": "Viimeisin yritys:",

    "update.errNoNetwork": "Ei yhteyttä. Kytke wifi päälle ja yritä uudelleen.",
    "update.errDownload": "Lataus epäonnistui.",
    "update.errResponse": "GitHub vastasi odottamattomasti.",
    "update.errNoAsset": "Uusin julkaisu ei sisällä asennettavaa tiedostoa.",
    "update.errUnsupported": "Tämä laiteohjelmisto ei osaa ladata päivitystä.",
    "update.errCorrupt": "Ladattu tiedosto on vioittunut — mitään ei muutettu.",
    "update.errHandover": "Sovellusta ei voitu vaihtaa. Uusi versio on täällä:",

    "date.months": ["Tammikuu", "Helmikuu", "Maaliskuu", "Huhtikuu", "Toukokuu", "Kesäkuu", "Heinäkuu", "Elokuu", "Syyskuu", "Lokakuu", "Marraskuu", "Joulukuu"],
    "date.monthsGen": ["tammikuuta", "helmikuuta", "maaliskuuta", "huhtikuuta", "toukokuuta", "kesäkuuta", "heinäkuuta", "elokuuta", "syyskuuta", "lokakuuta", "marraskuuta", "joulukuuta"],
    "date.weekdays": ["Ma", "Ti", "Ke", "To", "Pe", "La", "Su"],
    "date.dayMonth": "{d}. {monthGen}",

    "time.hm": "{h} t {m} min",
    "time.m": "{m} min",

    "plural.days": ["päivä", "päivää"]
};
