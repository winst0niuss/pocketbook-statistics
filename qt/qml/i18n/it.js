.pragma library

/* Italian catalog. Two plural forms. */

function plural(n) {
    return n === 1 ? 0 : 1;
}

var strings = {
    "app.title": "Statistiche",

    "nav.overview": "Panoramica",
    "nav.calendar": "Calendario",

    "overview.left": "Restano circa {time}",
    "overview.noBook": "Nessun libro ancora aperto",
    "overview.bookProgress": "Avanzamento: {percent} %",
    "overview.currentBook": "LIBRO ATTUALE",
    "overview.today": "OGGI",
    "overview.minutesToday": "minuti letti",
    "overview.allBooks": "TUTTI I LIBRI",
    "overview.booksFinished": "libri finiti",
    "overview.totalHours": "ore totali",
    "overview.pagesPerHour": "pagine all'ora",


    "calendar.dayTitle": "{date}  ·  {time}",
    "calendar.finished": "Finito",
    "calendar.trackingSince": "I dati di lettura sono registrati dal {date}.",
    "book.finishedOn": "Finito il {date}",
    "calendar.monthSummary": "{n} {days} · {time}",


    "edit.reset": "Reimposta",

    "streak.currentCaption": "giorni di fila ora",
    "streak.bestCaption": "serie migliore nel {year}",
    "streak.readDaysCaption": "{n} {days} di lettura nel {year}",
    "streak.notRead": "non letto",
    "streak.read": "letto",

    "about.section": "INFORMAZIONI",
    "about.streak": "Leggi da {n} {days} di fila!",
    "about.check": "Cerca aggiornamento",
    "about.install": "Installa {version}",
    "about.connecting": "Attivazione del Wi-Fi…",
    "about.checking": "Chiedo a GitHub l'ultima versione…",
    "about.uptodate": "Questa è l'ultima versione.",
    "about.available": "È disponibile la versione {version}.",
    "about.downloading": "Scarico l'aggiornamento…",
    "about.ready": "Aggiornamento scaricato. {app} si chiude e si riapre da solo — se non succede, aprilo dal menu delle applicazioni.",
    "about.autostart": "AVVIO AUTOMATICO",
    "about.log": "Ultimo tentativo:",

    "update.errNoNetwork": "Nessuna connessione. Attiva il Wi-Fi e riprova.",
    "update.errDownload": "Download non riuscito.",
    "update.errResponse": "GitHub ha risposto in modo inatteso.",
    "update.errNoAsset": "L'ultima versione non contiene un binario installabile.",
    "update.errUnsupported": "Questo firmware non offre modo di scaricare l'aggiornamento.",
    "update.errCorrupt": "Il file scaricato è danneggiato — non è stato cambiato nulla.",
    "update.errHandover": "Non è stato possibile sostituire l'applicazione. La nuova versione si trova qui:",

    "date.months": ["Gennaio", "Febbraio", "Marzo", "Aprile", "Maggio", "Giugno", "Luglio", "Agosto", "Settembre", "Ottobre", "Novembre", "Dicembre"],
    "date.monthsGen": ["gennaio", "febbraio", "marzo", "aprile", "maggio", "giugno", "luglio", "agosto", "settembre", "ottobre", "novembre", "dicembre"],
    "date.weekdays": ["Lun", "Mar", "Mer", "Gio", "Ven", "Sab", "Dom"],
    "date.dayMonth": "{d} {monthGen}",

    "time.hm": "{h} h {m} min",
    "time.m": "{m} min",

    "plural.days": ["giorno", "giorni"]
};
