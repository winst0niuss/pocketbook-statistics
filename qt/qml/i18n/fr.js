.pragma library

/* French catalog. Two plural forms; 0 stays singular. */

function plural(n) {
    return n <= 1 ? 0 : 1;
}

var strings = {
    "app.title": "Statistiques",

    "nav.overview": "Aperçu",
    "nav.calendar": "Calendrier",

    "overview.left": "Reste env. {time}",
    "overview.noBook": "Aucun livre ouvert pour l'instant",
    "overview.bookProgress": "Progression : {percent} %",
    "overview.currentBook": "LIVRE EN COURS",
    "overview.today": "AUJOURD'HUI",
    "overview.minutesToday": "minutes de lecture",
    "overview.allBooks": "TOUS LES LIVRES",
    "overview.booksFinished": "livres terminés",
    "overview.totalHours": "heures au total",
    "overview.pagesPerHour": "pages par heure",


    "calendar.dayTitle": "{date}  ·  {time}",
    "calendar.finished": "Terminé",
    "calendar.trackingSince": "Les données de lecture sont enregistrées depuis le {date}.",
    "book.finishedOn": "Terminé le {date}",
    "calendar.monthSummary": "{n} {days} · {time}",


    "streak.currentCaption": "jours d'affilée",
    "streak.bestCaption": "meilleure série en {year}",
    "streak.readDaysCaption": "{n} {days} de lecture en {year}",
    "streak.notRead": "non lu",
    "streak.read": "lu",

    "about.section": "À PROPOS",
    "about.streak": "Tu lis depuis {n} {days} d'affilée !",
    "about.check": "Rechercher une mise à jour",
    "about.install": "Installer {version}",
    "about.connecting": "Activation du Wi-Fi…",
    "about.checking": "Interrogation de GitHub sur la dernière version…",
    "about.uptodate": "C'est la dernière version.",
    "about.available": "La version {version} est disponible.",
    "about.downloading": "Téléchargement de la mise à jour…",
    "about.ready": "Mise à jour téléchargée. {app} se ferme et redémarre tout seul — sinon, ouvrez-le depuis le menu des applications.",
    "about.autostart": "DÉMARRAGE AUTO",
    "about.log": "Dernière tentative :",

    "update.errNoNetwork": "Pas de connexion. Activez le Wi-Fi et réessayez.",
    "update.errDownload": "Échec du téléchargement.",
    "update.errResponse": "GitHub a répondu de façon inattendue.",
    "update.errNoAsset": "La dernière version ne contient aucun binaire installable.",
    "update.errUnsupported": "Ce micrologiciel n'offre aucun moyen de télécharger la mise à jour.",
    "update.errCorrupt": "Le fichier téléchargé est endommagé — rien n'a été modifié.",
    "update.errHandover": "Impossible de remplacer l'application. La nouvelle version se trouve ici :",

    "date.months": ["Janvier", "Février", "Mars", "Avril", "Mai", "Juin", "Juillet", "Août", "Septembre", "Octobre", "Novembre", "Décembre"],
    "date.monthsGen": ["janvier", "février", "mars", "avril", "mai", "juin", "juillet", "août", "septembre", "octobre", "novembre", "décembre"],
    "date.weekdays": ["Lun", "Mar", "Mer", "Jeu", "Ven", "Sam", "Dim"],
    "date.dayMonth": "{d} {monthGen}",

    "time.hm": "{h} h {m} min",
    "time.m": "{m} min",

    "plural.days": ["jour", "jours"]
};
