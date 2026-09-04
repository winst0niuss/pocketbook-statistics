.pragma library

/* Turkish catalog. One noun form; the array holds it twice. */

function plural(n) {
    return n === 1 ? 0 : 1;
}

var strings = {
    "app.title": "İstatistikler",

    "nav.overview": "Genel bakış",
    "nav.calendar": "Takvim",

    "overview.left": "Yaklaşık {time} kaldı",
    "overview.noBook": "Henüz kitap açılmadı",
    "overview.bookProgress": "Kitap ilerlemesi: %{percent}",
    "overview.allBooks": "TÜM KİTAPLAR",
    "overview.booksFinished": "bitirilen kitaplar",
    "overview.totalHours": "toplam saat",
    "overview.pagesPerHour": "saatte sayfa",


    "calendar.dayTitle": "{date}  ·  {time}",
    "calendar.finished": "Bitirildi",
    "calendar.trackingSince": "Okuma verileri {date} tarihinden beri kaydediliyor.",
    "book.finishedOn": "{date} tarihinde bitirildi",
    "calendar.monthSummary": "{n} {days} · {time}",


    "streak.currentCaption": "gün üst üste",
    "streak.bestCaption": "{year} en iyi seri",
    "streak.readDaysCaption": "{year} yılında {n} {days} okuma",
    "streak.notRead": "okunmadı",
    "streak.read": "okundu",

    "about.section": "UYGULAMA HAKKINDA",
    "about.streak": "{n} {days} üst üste okuyorsun!",
    "about.check": "Güncelleme denetle",
    "about.install": "{version} sürümünü kur",
    "about.connecting": "Bringing Wi-Fi up…",
    "about.checking": "GitHub'a son sürüm soruluyor…",
    "about.uptodate": "Bu en son sürüm.",
    "about.available": "{version} sürümü mevcut.",
    "about.downloading": "Güncelleme indiriliyor…",
    "about.ready": "Güncelleme indirildi. {app} kapanıp kendi kendine açılır — açılmazsa uygulamalar menüsünden başlatın.",
    "about.autostart": "OTOMATİK BAŞLATMA",
    "about.log": "Son deneme:",

    "update.errNoNetwork": "Bağlantı yok. Wi-Fi'yi açıp yeniden deneyin.",
    "update.errDownload": "İndirme başarısız oldu.",
    "update.errResponse": "GitHub beklenmedik bir yanıt verdi.",
    "update.errNoAsset": "Son sürümde kurulabilir dosya yok.",
    "update.errUnsupported": "Bu ürün yazılımı güncellemeyi indiremiyor.",
    "update.errCorrupt": "İndirilen dosya bozuk — hiçbir şey değiştirilmedi.",
    "update.errHandover": "Uygulama değiştirilemedi. Yeni sürüm burada:",

    "date.months": ["Ocak", "Şubat", "Mart", "Nisan", "Mayıs", "Haziran", "Temmuz", "Ağustos", "Eylül", "Ekim", "Kasım", "Aralık"],
    "date.monthsGen": ["Ocak", "Şubat", "Mart", "Nisan", "Mayıs", "Haziran", "Temmuz", "Ağustos", "Eylül", "Ekim", "Kasım", "Aralık"],
    "date.weekdays": ["Pzt", "Sal", "Çar", "Per", "Cum", "Cmt", "Paz"],
    "date.dayMonth": "{d} {monthGen}",

    "time.hm": "{h} sa {m} dk",
    "time.m": "{m} dk",

    "plural.days": ["gün", "gün"]
};
