.pragma library

/* Kazakh catalog. Two plural forms; the noun stays singular after a number. */

function plural(n) {
    return n === 1 ? 0 : 1;
}

var strings = {
    "app.title": "Статистика",

    "nav.overview": "Шолу",
    "nav.calendar": "Күнтізбе",

    "overview.left": "Шамамен {time} қалды",
    "overview.noBook": "Әзірге кітап ашылмаған",
    "overview.bookProgress": "Кітап барысы: {percent} %",
    "overview.allBooks": "БАРЛЫҚ КІТАПТАР",
    "overview.booksFinished": "оқып бітірген кітаптар",
    "overview.totalHours": "барлығы сағат",
    "overview.pagesPerHour": "сағатына бет",


    "calendar.dayTitle": "{date}  ·  {time}",
    "calendar.finished": "Оқып бітті",
    "calendar.trackingSince": "Оқу деректері {date} бастап жиналуда.",
    "book.finishedOn": "{date} оқып бітті",
    "calendar.monthSummary": "{n} {days} · {time}",


    "edit.ok": "ОК",
    "edit.hoursTitle": "БАРЛЫҒЫ ОҚУ САҒАТЫ",
    "edit.booksTitle": "БАРЛЫҒЫ ОҚЫЛҒАН КІТАП",

    "streak.currentCaption": "күн қатарынан",
    "streak.bestCaption": "{year} үздік серия",
    "streak.readDaysCaption": "{year} жылы {n} {days} оқу",
    "streak.notRead": "оқылмаған",
    "streak.read": "оқылған",

    "about.section": "ҚОСЫМША ТУРАЛЫ",
    "about.streak": "{n} {days} қатарынан оқып жүрсің!",
    "about.check": "Жаңартуды тексеру",
    "about.install": "{version} орнату",
    "about.connecting": "Bringing Wi-Fi up…",
    "about.checking": "GitHub-тан соңғы шығарылымды сұраудамын…",
    "about.uptodate": "Бұл ең соңғы нұсқа.",
    "about.available": "{version} нұсқасы қолжетімді.",
    "about.downloading": "Жаңарту жүктелуде…",
    "about.ready": "Жаңарту жүктелді. {app} жабылып, өзі қайта іске қосылады — қосылмаса, оны қолданбалар мәзірінен ашыңыз.",
    "about.autostart": "АВТОМАТТЫ ІСКЕ ҚОСУ",
    "about.log": "Соңғы әрекет:",

    "update.errNoNetwork": "Байланыс жоқ. Wi-Fi қосып, қайталап көріңіз.",
    "update.errDownload": "Жүктеу сәтсіз аяқталды.",
    "update.errResponse": "GitHub күтпеген жауап берді.",
    "update.errNoAsset": "Соңғы шығарылымда орнатылатын файл жоқ.",
    "update.errUnsupported": "Бұл микробағдарлама жаңартуды жүктей алмайды.",
    "update.errCorrupt": "Жүктелген файл бүлінген — ештеңе өзгертілген жоқ.",
    "update.errHandover": "Қолданбаны ауыстыру мүмкін болмады. Жаңа нұсқа мұнда:",

    "date.months": ["Қаңтар", "Ақпан", "Наурыз", "Сәуір", "Мамыр", "Маусым", "Шілде", "Тамыз", "Қыркүйек", "Қазан", "Қараша", "Желтоқсан"],
    "date.monthsGen": ["қаңтар", "ақпан", "наурыз", "сәуір", "мамыр", "маусым", "шілде", "тамыз", "қыркүйек", "қазан", "қараша", "желтоқсан"],
    "date.weekdays": ["Дс", "Сс", "Ср", "Бс", "Жм", "Сн", "Жс"],
    "date.dayMonth": "{d} {monthGen}",

    "time.hm": "{h} сағ {m} мин",
    "time.m": "{m} мин",

    "plural.days": ["күн", "күн"]
};
