.pragma library

/* Azerbaijani catalog. Two plural forms; the noun stays singular after a number. */

function plural(n) {
    return n === 1 ? 0 : 1;
}

var strings = {
    "app.title": "Statistika",

    "nav.overview": "İcmal",
    "nav.calendar": "Təqvim",

    "overview.left": "Təxminən {time} qalıb",
    "overview.noBook": "Hələ heç bir kitab açılmayıb",
    "overview.bookProgress": "Kitab irəliləyişi: {percent} %",
    "overview.allBooks": "BÜTÜN KİTABLAR",
    "overview.booksFinished": "bitirilmiş kitablar",
    "overview.totalHours": "ümumi saat",
    "overview.pagesPerHour": "saatda səhifə",


    "calendar.dayTitle": "{date}  ·  {time}",
    "calendar.finished": "Bitirilib",
    "calendar.trackingSince": "Oxu məlumatları {date} tarixindən yazılır.",
    "book.finishedOn": "{date} tarixində bitirilib",
    "calendar.monthSummary": "{n} {days} · {time}",


    "streak.currentCaption": "gün ardıcıl",
    "streak.bestCaption": "{year} ən yaxşı seriya",
    "streak.readDaysCaption": "{year} ildə {n} {days} oxu",
    "streak.notRead": "oxunmayıb",
    "streak.read": "oxundu",

    "about.section": "TƏTBİQ HAQQINDA",
    "about.streak": "{n} {days} ardıcıl oxuyursan!",
    "about.check": "Yeniləməni yoxla",
    "about.install": "{version} quraşdır",
    "about.connecting": "Bringing Wi-Fi up…",
    "about.checking": "GitHub-dan son buraxılış soruşulur…",
    "about.uptodate": "Bu ən son versiyadır.",
    "about.available": "{version} versiyası mövcuddur.",
    "about.downloading": "Yeniləmə endirilir…",
    "about.ready": "Yeniləmə endirildi. {app} bağlanıb özü yenidən açılacaq — açılmasa, onu proqramlar menyusundan başladın.",
    "about.autostart": "AVTOMATIK BAŞLATMA",
    "about.log": "Son cəhd:",

    "update.errNoNetwork": "Bağlantı yoxdur. Wi-Fi-ı açıb yenidən cəhd edin.",
    "update.errDownload": "Endirmə alınmadı.",
    "update.errResponse": "GitHub gözlənilməz cavab verdi.",
    "update.errNoAsset": "Son buraxılışda quraşdırıla bilən fayl yoxdur.",
    "update.errUnsupported": "Bu proqram təminatı yeniləməni endirə bilmir.",
    "update.errCorrupt": "Endirilən fayl zədələnib — heç nə dəyişdirilmədi.",
    "update.errHandover": "Proqramı əvəz etmək mümkün olmadı. Yeni versiya buradadır:",

    "date.months": ["Yanvar", "Fevral", "Mart", "Aprel", "May", "İyun", "İyul", "Avqust", "Sentyabr", "Oktyabr", "Noyabr", "Dekabr"],
    "date.monthsGen": ["yanvar", "fevral", "mart", "aprel", "may", "iyun", "iyul", "avqust", "sentyabr", "oktyabr", "noyabr", "dekabr"],
    "date.weekdays": ["B.e", "Ç.a", "Çər", "C.a", "Cüm", "Şən", "Baz"],
    "date.dayMonth": "{d} {monthGen}",

    "time.hm": "{h} saat {m} dəq",
    "time.m": "{m} dəq",

    "plural.days": ["gün", "gün"]
};
