.pragma library

/* Russian catalog. Three plural forms: 1 книга, 2 книги, 5 книг. */

function plural(n) {
    var mod10 = n % 10;
    var mod100 = n % 100;
    if (mod10 === 1 && mod100 !== 11)
        return 0;
    if (mod10 >= 2 && mod10 <= 4 && (mod100 < 12 || mod100 > 14))
        return 1;
    return 2;
}

var strings = {
    "app.title": "Статистика",

    "nav.overview": "Обзор",
    "nav.calendar": "Календарь",

    "overview.left": "Осталось около {time}",
    "overview.noBook": "Книга ещё не открыта",
    "overview.bookProgress": "Прогресс книги: {percent} %",
    "overview.currentBook": "ТЕКУЩАЯ КНИГА",
    "overview.today": "СЕГОДНЯ",
    "overview.minutesToday": "минут чтения",
    "overview.allBooks": "ВСЕ КНИГИ",
    "overview.booksFinished": "книг прочитано",
    "overview.totalHours": "часов всего",
    "overview.pagesPerHour": "страниц в час",


    "calendar.dayTitle": "{date}  ·  {time}",
    "calendar.finished": "Дочитано",
    "calendar.trackingSince": "Данные о чтении собираются с {date}.",
    "book.finishedOn": "Дочитано {date}",
    "calendar.monthSummary": "{n} {days} · {time}",


    "edit.reset": "Сбросить",

    "streak.currentCaption": "дней подряд сейчас",
    "streak.bestCaption": "лучшая серия в {year}",
    "streak.readDaysCaption": "{n} {days} чтения в {year}",
    "streak.notRead": "без чтения",
    "streak.read": "чтение",

    "about.section": "О ПРИЛОЖЕНИИ",
    "about.streak": "Ты читаешь уже {n} {days} подряд!",
    "about.check": "Проверить обновление",
    "about.install": "Установить {version}",
    "about.connecting": "Поднимаю Wi-Fi…",
    "about.checking": "Спрашиваю GitHub о последнем релизе\u2026",
    "about.uptodate": "Установлена последняя версия.",
    "about.available": "Доступна версия {version}.",
    "about.downloading": "Загружаю обновление\u2026",
    "about.ready": "Обновление загружено. {app} закроется и запустится сам — если этого не произошло, откройте его из меню приложений.",

    "about.autostart": "АВТОЗАПУСК",
    "about.shim": "Запуск статистики при открытии книги",
    "about.shimHint": "Открытие EPUB, FB2 или PDF запускает учёт статистики.",
    "about.log": "Последняя попытка:",

    "update.errNoNetwork": "Нет соединения. Включите Wi-Fi и повторите.",
    "update.errDownload": "Не удалось загрузить.",
    "update.errResponse": "GitHub ответил неожиданно.",
    "update.errNoAsset": "В последнем релизе нет готовой сборки.",
    "update.errUnsupported": "Прошивка не даёт способа загрузить обновление.",
    "update.errCorrupt": "Загруженный файл повреждён — ничего не изменено.",
    "update.errHandover": "Не удалось подменить приложение. Новая версия лежит здесь:",

    "date.months": ["Январь", "Февраль", "Март", "Апрель", "Май", "Июнь", "Июль",
                    "Август", "Сентябрь", "Октябрь", "Ноябрь", "Декабрь"],
    "date.monthsGen": ["января", "февраля", "марта", "апреля", "мая", "июня", "июля",
                       "августа", "сентября", "октября", "ноября", "декабря"],
    "date.weekdays": ["Пн", "Вт", "Ср", "Чт", "Пт", "Сб", "Вс"],
    "date.dayMonth": "{d} {monthGen}",

    "time.hm": "{h} ч {m} мин",
    "time.m": "{m} мин",

    "plural.days": ["день", "дня", "дней"],
    "plural.minutesToday": ["минута чтения", "минуты чтения", "минут чтения"],
    "plural.booksFinished": ["книга прочитана", "книги прочитаны", "книг прочитано"],
    "plural.totalHours": ["час всего", "часа всего", "часов всего"]
};
