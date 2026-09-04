.pragma library

/* Spanish catalog. Two plural forms. */

function plural(n) {
    return n === 1 ? 0 : 1;
}

var strings = {
    "app.title": "Estadísticas",

    "nav.overview": "Resumen",
    "nav.calendar": "Calendario",

    "overview.left": "Quedan aprox. {time}",
    "overview.noBook": "Aún no se ha abierto ningún libro",
    "overview.bookProgress": "Progreso del libro: {percent} %",
    "overview.currentBook": "LIBRO ACTUAL",
    "overview.today": "HOY",
    "overview.minutesToday": "minutos leídos",
    "overview.allBooks": "TODOS LOS LIBROS",
    "overview.booksFinished": "libros terminados",
    "overview.totalHours": "horas en total",
    "overview.pagesPerHour": "páginas por hora",


    "calendar.dayTitle": "{date}  ·  {time}",
    "calendar.finished": "Terminado",
    "calendar.trackingSince": "Los datos de lectura se registran desde el {date}.",
    "book.finishedOn": "Terminado el {date}",
    "calendar.monthSummary": "{n} {days} · {time}",


    "streak.currentCaption": "días seguidos ahora",
    "streak.bestCaption": "mejor racha en {year}",
    "streak.readDaysCaption": "{n} {days} de lectura en {year}",
    "streak.notRead": "sin lectura",
    "streak.read": "leído",

    "about.section": "ACERCA DE",
    "about.streak": "¡Llevas {n} {days} seguidos leyendo!",
    "about.check": "Buscar actualización",
    "about.install": "Instalar {version}",
    "about.connecting": "Activando el Wi-Fi…",
    "about.checking": "Consultando la última versión en GitHub…",
    "about.uptodate": "Esta es la última versión.",
    "about.available": "La versión {version} está disponible.",
    "about.downloading": "Descargando la actualización…",
    "about.ready": "Actualización descargada. {app} se cierra y se abre de nuevo solo — si no lo hace, ábrelo desde el menú de aplicaciones.",
    "about.autostart": "INICIO AUTOMÁTICO",
    "about.log": "Último intento:",

    "update.errNoNetwork": "Sin conexión. Activa el Wi-Fi e inténtalo de nuevo.",
    "update.errDownload": "La descarga ha fallado.",
    "update.errResponse": "GitHub respondió de forma inesperada.",
    "update.errNoAsset": "La última versión no incluye ningún binario instalable.",
    "update.errUnsupported": "Este firmware no ofrece forma de descargar la actualización.",
    "update.errCorrupt": "El archivo descargado está dañado — no se ha cambiado nada.",
    "update.errHandover": "No se pudo sustituir la aplicación. La nueva versión está aquí:",

    "date.months": ["Enero", "Febrero", "Marzo", "Abril", "Mayo", "Junio", "Julio", "Agosto", "Septiembre", "Octubre", "Noviembre", "Diciembre"],
    "date.monthsGen": ["enero", "febrero", "marzo", "abril", "mayo", "junio", "julio", "agosto", "septiembre", "octubre", "noviembre", "diciembre"],
    "date.weekdays": ["Lun", "Mar", "Mié", "Jue", "Vie", "Sáb", "Dom"],
    "date.dayMonth": "{d} de {monthGen}",

    "time.hm": "{h} h {m} min",
    "time.m": "{m} min",

    "plural.days": ["día", "días"]
};
