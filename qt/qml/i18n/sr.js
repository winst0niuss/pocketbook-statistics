.pragma library

/* Serbian catalog. Three plural forms: 1 књига, 2 књиге, 5 књига. */

function plural(n) {
    var m10 = n % 10, m100 = n % 100;
    if (m10 === 1 && m100 !== 11)
        return 0;
    if (m10 >= 2 && m10 <= 4 && (m100 < 12 || m100 > 14))
        return 1;
    return 2;
}

var strings = {
    "app.title": "Статистика",

    "nav.overview": "Преглед",
    "nav.calendar": "Календар",

    "overview.left": "Остало око {time}",
    "overview.noBook": "Још ниједна књига није отворена",
    "overview.bookProgress": "Напредак књиге: {percent} %",
    "overview.allBooks": "СВЕ КЊИГЕ",
    "overview.booksFinished": "прочитане књиге",
    "overview.totalHours": "сати укупно",
    "overview.pagesPerHour": "страна на сат",


    "calendar.dayTitle": "{date}  ·  {time}",
    "calendar.finished": "Прочитана",
    "calendar.trackingSince": "Подаци о читању бележе се од {date}.",
    "book.finishedOn": "Прочитана {date}",
    "calendar.monthSummary": "{n} {days} · {time}",


    "edit.ok": "ОК",
    "edit.hoursTitle": "УКУПНО САТИ ЧИТАЊА",
    "edit.booksTitle": "УКУПНО ПРОЧИТАНИХ КЊИГА",

    "streak.currentCaption": "дана заредом сада",
    "streak.bestCaption": "најбоља серија {year}",
    "streak.readDaysCaption": "{n} {days} читања у {year}",
    "streak.notRead": "без читања",
    "streak.read": "читање",

    "about.section": "О АПЛИКАЦИЈИ",
    "about.streak": "Читаш већ {n} {days} заредом!",
    "about.check": "Провери ажурирање",
    "about.install": "Инсталирај {version}",
    "about.connecting": "Bringing Wi-Fi up…",
    "about.checking": "Питам GitHub за најновије издање…",
    "about.uptodate": "Ово је најновија верзија.",
    "about.available": "Доступна је верзија {version}.",
    "about.downloading": "Преузимам ажурирање…",
    "about.ready": "Ажурирање је преузето. {app} ће се затворити и сам поново покренути — ако не, отворите га из менија апликација.",
    "about.autostart": "АУТОМАТСКО ПОКРЕТАЊЕ",
    "about.log": "Последњи покушај:",

    "update.errNoNetwork": "Нема везе. Укључите Wi-Fi и покушајте поново.",
    "update.errDownload": "Преузимање није успело.",
    "update.errResponse": "GitHub је одговорио неочекивано.",
    "update.errNoAsset": "Најновије издање нема датотеку за инсталирање.",
    "update.errUnsupported": "Овај фирмвер не може да преузме ажурирање.",
    "update.errCorrupt": "Преузета датотека је оштећена — ништа није промењено.",
    "update.errHandover": "Апликација није могла да се замени. Нова верзија је овде:",

    "date.months": ["Јануар", "Фебруар", "Март", "Април", "Мај", "Јун", "Јул", "Август", "Септембар", "Октобар", "Новембар", "Децембар"],
    "date.monthsGen": ["јануара", "фебруара", "марта", "априла", "маја", "јуна", "јула", "августа", "септембра", "октобра", "новембра", "децембра"],
    "date.weekdays": ["Пон", "Уто", "Сре", "Чет", "Пет", "Суб", "Нед"],
    "date.dayMonth": "{d}. {monthGen}",

    "time.hm": "{h} ч {m} мин",
    "time.m": "{m} мин",

    "plural.days": ["дан", "дана", "дана"]
};
