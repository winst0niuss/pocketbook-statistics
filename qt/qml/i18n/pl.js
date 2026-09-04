.pragma library

/* Polish catalog. Three plural forms: 1 książka, 2 książki, 5 książek. */

function plural(n) {
    var m10 = n % 10, m100 = n % 100;
    if (n === 1)
        return 0;
    if (m10 >= 2 && m10 <= 4 && (m100 < 12 || m100 > 14))
        return 1;
    return 2;
}

var strings = {
    "app.title": "Statystyki",

    "nav.overview": "Przegląd",
    "nav.calendar": "Kalendarz",

    "overview.left": "Zostało ok. {time}",
    "overview.noBook": "Nie otwarto jeszcze książki",
    "overview.bookProgress": "Postęp książki: {percent} %",
    "overview.currentBook": "BIEŻĄCA KSIĄŻKA",
    "overview.today": "DZISIAJ",
    "overview.minutesToday": "minut czytania",
    "overview.allBooks": "WSZYSTKIE KSIĄŻKI",
    "overview.booksFinished": "książek przeczytanych",
    "overview.totalHours": "godzin łącznie",
    "overview.pagesPerHour": "stron na godzinę",


    "calendar.dayTitle": "{date}  ·  {time}",
    "calendar.finished": "Przeczytana",
    "calendar.trackingSince": "Dane o czytaniu są zapisywane od {date}.",
    "book.finishedOn": "Przeczytana {date}",
    "calendar.monthSummary": "{n} {days} · {time}",


    "edit.reset": "Przywróć",

    "streak.currentCaption": "dni z rzędu teraz",
    "streak.bestCaption": "najlepsza seria w {year}",
    "streak.readDaysCaption": "{n} {days} czytania w {year}",
    "streak.notRead": "bez czytania",
    "streak.read": "czytanie",

    "about.section": "O APLIKACJI",
    "about.streak": "Czytasz już {n} {days} z rzędu!",
    "about.check": "Sprawdź aktualizację",
    "about.install": "Zainstaluj {version}",
    "about.connecting": "Włączanie Wi-Fi…",
    "about.checking": "Pytam GitHub o najnowsze wydanie…",
    "about.uptodate": "To jest najnowsza wersja.",
    "about.available": "Dostępna jest wersja {version}.",
    "about.downloading": "Pobieram aktualizację…",
    "about.ready": "Aktualizacja pobrana. {app} zamknie się i uruchomi ponownie sam — jeśli nie, otwórz go z menu aplikacji.",
    "about.autostart": "AUTOSTART",
    "about.log": "Ostatnia próba:",

    "update.errNoNetwork": "Brak połączenia. Włącz Wi-Fi i spróbuj ponownie.",
    "update.errDownload": "Pobieranie nie powiodło się.",
    "update.errResponse": "GitHub odpowiedział nieoczekiwanie.",
    "update.errNoAsset": "Najnowsze wydanie nie zawiera gotowej wersji.",
    "update.errUnsupported": "To oprogramowanie nie pozwala pobrać aktualizacji.",
    "update.errCorrupt": "Pobrany plik jest uszkodzony — nic nie zmieniono.",
    "update.errHandover": "Nie udało się podmienić aplikacji. Nowa wersja jest tutaj:",

    "date.months": ["Styczeń", "Luty", "Marzec", "Kwiecień", "Maj", "Czerwiec", "Lipiec", "Sierpień", "Wrzesień", "Październik", "Listopad", "Grudzień"],
    "date.monthsGen": ["stycznia", "lutego", "marca", "kwietnia", "maja", "czerwca", "lipca", "sierpnia", "września", "października", "listopada", "grudnia"],
    "date.weekdays": ["Pon", "Wto", "Śro", "Czw", "Pią", "Sob", "Nie"],
    "date.dayMonth": "{d} {monthGen}",

    "time.hm": "{h} godz {m} min",
    "time.m": "{m} min",

    "plural.days": ["dzień", "dni", "dni"]
};
