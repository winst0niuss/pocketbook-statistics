.pragma library

/* Portuguese catalog. Two plural forms. */

function plural(n) {
    return n === 1 ? 0 : 1;
}

var strings = {
    "app.title": "Estatísticas",

    "nav.overview": "Resumo",
    "nav.calendar": "Calendário",

    "overview.left": "Faltam cerca de {time}",
    "overview.noBook": "Ainda não abriu nenhum livro",
    "overview.bookProgress": "Progresso do livro: {percent} %",
    "overview.allBooks": "TODOS OS LIVROS",
    "overview.booksFinished": "livros terminados",
    "overview.totalHours": "horas no total",
    "overview.pagesPerHour": "páginas por hora",


    "calendar.dayTitle": "{date}  ·  {time}",
    "calendar.finished": "Terminado",
    "calendar.trackingSince": "Os dados de leitura são registados desde {date}.",
    "book.finishedOn": "Terminado a {date}",
    "calendar.monthSummary": "{n} {days} · {time}",


    "edit.ok": "OK",
    "edit.hoursTitle": "HORAS DE LEITURA NO TOTAL",
    "edit.booksTitle": "LIVROS TERMINADOS NO TOTAL",

    "streak.currentCaption": "dias seguidos agora",
    "streak.bestCaption": "melhor série em {year}",
    "streak.readDaysCaption": "{n} {days} de leitura em {year}",
    "streak.notRead": "sem leitura",
    "streak.read": "lido",

    "about.section": "SOBRE",
    "about.streak": "Já leste {n} {days} seguidos!",
    "about.check": "Procurar atualização",
    "about.install": "Instalar {version}",
    "about.connecting": "Bringing Wi-Fi up…",
    "about.checking": "A perguntar ao GitHub pela versão mais recente…",
    "about.uptodate": "Esta é a versão mais recente.",
    "about.available": "A versão {version} está disponível.",
    "about.downloading": "A transferir a atualização…",
    "about.ready": "Atualização transferida. O {app} fecha-se e abre de novo sozinho — se não abrir, abra-o pelo menu de aplicações.",
    "about.autostart": "ARRANQUE AUTOMÁTICO",
    "about.log": "Última tentativa:",

    "update.errNoNetwork": "Sem ligação. Ligue o Wi-Fi e tente de novo.",
    "update.errDownload": "A transferência falhou.",
    "update.errResponse": "O GitHub respondeu de forma inesperada.",
    "update.errNoAsset": "A versão mais recente não inclui um binário instalável.",
    "update.errUnsupported": "Este firmware não oferece forma de transferir a atualização.",
    "update.errCorrupt": "O ficheiro transferido está danificado — nada foi alterado.",
    "update.errHandover": "Não foi possível substituir a aplicação. A nova versão está aqui:",

    "date.months": ["Janeiro", "Fevereiro", "Março", "Abril", "Maio", "Junho", "Julho", "Agosto", "Setembro", "Outubro", "Novembro", "Dezembro"],
    "date.monthsGen": ["janeiro", "fevereiro", "março", "abril", "maio", "junho", "julho", "agosto", "setembro", "outubro", "novembro", "dezembro"],
    "date.weekdays": ["Seg", "Ter", "Qua", "Qui", "Sex", "Sáb", "Dom"],
    "date.dayMonth": "{d} de {monthGen}",

    "time.hm": "{h} h {m} min",
    "time.m": "{m} min",

    "plural.days": ["dia", "dias"]
};
