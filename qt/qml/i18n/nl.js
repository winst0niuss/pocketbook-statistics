.pragma library

/* Dutch catalog. Two plural forms. */

function plural(n) {
    return n === 1 ? 0 : 1;
}

var strings = {
    "app.title": "Statistieken",

    "nav.overview": "Overzicht",
    "nav.calendar": "Kalender",

    "overview.left": "Nog ong. {time}",
    "overview.noBook": "Nog geen boek geopend",
    "overview.bookProgress": "Voortgang: {percent} %",
    "overview.allBooks": "ALLE BOEKEN",
    "overview.booksFinished": "boeken uitgelezen",
    "overview.totalHours": "uren totaal",
    "overview.pagesPerHour": "pagina's per uur",


    "calendar.dayTitle": "{date}  ·  {time}",
    "calendar.finished": "Uitgelezen",
    "calendar.trackingSince": "Leesgegevens worden vastgelegd sinds {date}.",
    "book.finishedOn": "Uitgelezen op {date}",
    "calendar.monthSummary": "{n} {days} · {time}",


    "streak.currentCaption": "dagen op rij nu",
    "streak.bestCaption": "beste reeks in {year}",
    "streak.readDaysCaption": "{n} {days} gelezen in {year}",
    "streak.notRead": "niet gelezen",
    "streak.read": "gelezen",

    "about.section": "OVER DE APP",
    "about.streak": "Je leest al {n} {days} op rij!",
    "about.check": "Controleren op update",
    "about.install": "{version} installeren",
    "about.connecting": "Bringing Wi-Fi up…",
    "about.checking": "GitHub om de nieuwste release vragen…",
    "about.uptodate": "Dit is de nieuwste versie.",
    "about.available": "Versie {version} is beschikbaar.",
    "about.downloading": "Update downloaden…",
    "about.ready": "Update gedownload. {app} sluit en start vanzelf opnieuw — zo niet, open het dan via het menu.",
    "about.autostart": "AUTOSTART",
    "about.log": "Laatste poging:",

    "update.errNoNetwork": "Geen verbinding. Zet wifi aan en probeer opnieuw.",
    "update.errDownload": "Downloaden mislukt.",
    "update.errResponse": "GitHub gaf een onverwacht antwoord.",
    "update.errNoAsset": "De nieuwste release bevat geen installeerbare build.",
    "update.errUnsupported": "Deze firmware biedt geen manier om de update te downloaden.",
    "update.errCorrupt": "Het gedownloade bestand is beschadigd — er is niets gewijzigd.",
    "update.errHandover": "De nieuwe versie kon niet worden geplaatst. Ze staat hier:",

    "date.months": ["Januari", "Februari", "Maart", "April", "Mei", "Juni", "Juli", "Augustus", "September", "Oktober", "November", "December"],
    "date.monthsGen": ["januari", "februari", "maart", "april", "mei", "juni", "juli", "augustus", "september", "oktober", "november", "december"],
    "date.weekdays": ["Ma", "Di", "Wo", "Do", "Vr", "Za", "Zo"],
    "date.dayMonth": "{d} {monthGen}",

    "time.hm": "{h} u {m} min",
    "time.m": "{m} min",

    "plural.days": ["dag", "dagen"]
};
