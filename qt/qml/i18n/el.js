.pragma library

/* Greek catalog. Two plural forms. */

function plural(n) {
    return n === 1 ? 0 : 1;
}

var strings = {
    "app.title": "Στατιστικά",

    "nav.overview": "Επισκόπηση",
    "nav.calendar": "Ημερολόγιο",

    "overview.left": "Απομένουν περ. {time}",
    "overview.noBook": "Δεν έχει ανοιχτεί ακόμη βιβλίο",
    "overview.bookProgress": "Πρόοδος βιβλίου: {percent} %",
    "overview.allBooks": "ΟΛΑ ΤΑ ΒΙΒΛΙΑ",
    "overview.booksFinished": "ολοκληρωμένα βιβλία",
    "overview.totalHours": "ώρες συνολικά",
    "overview.pagesPerHour": "σελίδες την ώρα",


    "calendar.dayTitle": "{date}  ·  {time}",
    "calendar.finished": "Ολοκληρώθηκε",
    "calendar.trackingSince": "Τα δεδομένα ανάγνωσης καταγράφονται από {date}.",
    "book.finishedOn": "Ολοκληρώθηκε {date}",
    "calendar.monthSummary": "{n} {days} · {time}",


    "streak.currentCaption": "μέρες στη σειρά",
    "streak.bestCaption": "καλύτερο σερί το {year}",
    "streak.readDaysCaption": "{n} {days} ανάγνωσης το {year}",
    "streak.notRead": "χωρίς ανάγνωση",
    "streak.read": "ανάγνωση",

    "about.section": "ΣΧΕΤΙΚΑ",
    "about.streak": "Διαβάζεις {n} {days} στη σειρά!",
    "about.check": "Έλεγχος ενημέρωσης",
    "about.install": "Εγκατάσταση {version}",
    "about.connecting": "Bringing Wi-Fi up…",
    "about.checking": "Ρωτάω το GitHub για την τελευταία έκδοση…",
    "about.uptodate": "Αυτή είναι η τελευταία έκδοση.",
    "about.available": "Η έκδοση {version} είναι διαθέσιμη.",
    "about.downloading": "Λήψη ενημέρωσης…",
    "about.ready": "Η ενημέρωση κατέβηκε. Το {app} κλείνει και ξεκινά μόνο του — αν δεν ξεκινήσει, ανοίξτε το από το μενού εφαρμογών.",
    "about.autostart": "ΑΥΤΟΜΑΤΗ ΕΚΚΙΝΗΣΗ",
    "about.log": "Τελευταία προσπάθεια:",

    "update.errNoNetwork": "Καμία σύνδεση. Ενεργοποιήστε το Wi-Fi και δοκιμάστε ξανά.",
    "update.errDownload": "Η λήψη απέτυχε.",
    "update.errResponse": "Το GitHub απάντησε απροσδόκητα.",
    "update.errNoAsset": "Η τελευταία έκδοση δεν περιλαμβάνει εκτελέσιμο αρχείο.",
    "update.errUnsupported": "Αυτό το firmware δεν μπορεί να κατεβάσει την ενημέρωση.",
    "update.errCorrupt": "Το αρχείο που κατέβηκε είναι κατεστραμμένο — δεν άλλαξε τίποτα.",
    "update.errHandover": "Η εφαρμογή δεν μπόρεσε να αντικατασταθεί. Η νέα έκδοση βρίσκεται εδώ:",

    "date.months": ["Ιανουάριος", "Φεβρουάριος", "Μάρτιος", "Απρίλιος", "Μάιος", "Ιούνιος", "Ιούλιος", "Αύγουστος", "Σεπτέμβριος", "Οκτώβριος", "Νοέμβριος", "Δεκέμβριος"],
    "date.monthsGen": ["Ιανουαρίου", "Φεβρουαρίου", "Μαρτίου", "Απριλίου", "Μαΐου", "Ιουνίου", "Ιουλίου", "Αυγούστου", "Σεπτεμβρίου", "Οκτωβρίου", "Νοεμβρίου", "Δεκεμβρίου"],
    "date.weekdays": ["Δε", "Τρ", "Τε", "Πε", "Πα", "Σα", "Κυ"],
    "date.dayMonth": "{d} {monthGen}",

    "time.hm": "{h} ώ {m} λ",
    "time.m": "{m} λεπτά",

    "plural.days": ["ημέρα", "ημέρες"]
};
