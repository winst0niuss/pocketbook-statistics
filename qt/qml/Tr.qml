pragma Singleton
import QtQuick

import "i18n/az.js" as LangAz
import "i18n/bg.js" as LangBg
import "i18n/cs.js" as LangCs
import "i18n/da.js" as LangDa
import "i18n/de.js" as LangDe
import "i18n/el.js" as LangEl
import "i18n/en.js" as LangEn
import "i18n/es.js" as LangEs
import "i18n/et.js" as LangEt
import "i18n/fi.js" as LangFi
import "i18n/fr.js" as LangFr
import "i18n/hr.js" as LangHr
import "i18n/hu.js" as LangHu
import "i18n/it.js" as LangIt
import "i18n/kk.js" as LangKk
import "i18n/lt.js" as LangLt
import "i18n/lv.js" as LangLv
import "i18n/nb.js" as LangNb
import "i18n/nl.js" as LangNl
import "i18n/pl.js" as LangPl
import "i18n/pt.js" as LangPt
import "i18n/ro.js" as LangRo
import "i18n/ru.js" as LangRu
import "i18n/sk.js" as LangSk
import "i18n/sl.js" as LangSl
import "i18n/sr.js" as LangSr
import "i18n/sv.js" as LangSv
import "i18n/tr.js" as LangTr
import "i18n/uk.js" as LangUk

/* Key-based localization. To add a language: copy a catalog in i18n/, list it
 * in pocketbook-statistics.qrc, then add one import above and one row to the table below —
 * call sites never change. A key a catalog is missing falls back to English,
 * and so does a device language no catalog covers. The launcher tile's label
 * is the one string outside this system; see launcherTitle() in installer.cpp. */
QtObject {
    /* Two-letter code -> catalog. The device reports things like "ru", "de-DE"
     * or "pt_BR", so only the first two letters are matched; a language with no
     * catalog falls back to English rather than showing keys. */
    readonly property var catalogs: ({
        "az": LangAz,
        "bg": LangBg,
        "cs": LangCs,
        "da": LangDa,
        "de": LangDe,
        "el": LangEl,
        "en": LangEn,
        "es": LangEs,
        "et": LangEt,
        "fi": LangFi,
        "fr": LangFr,
        "hr": LangHr,
        "hu": LangHu,
        "it": LangIt,
        "kk": LangKk,
        "lt": LangLt,
        "lv": LangLv,
        "nb": LangNb,
        "nl": LangNl,
        "pl": LangPl,
        "pt": LangPt,
        "ro": LangRo,
        "ru": LangRu,
        "sk": LangSk,
        "sl": LangSl,
        "sr": LangSr,
        "sv": LangSv,
        "tr": LangTr,
        "uk": LangUk,
        "no": LangNb  /* the reader may report either code for Norwegian */
    })

    readonly property var catalog: {
        var raw = (typeof deviceLang === "undefined") ? "" : (deviceLang || "");
        var code = raw.substring(0, 2).toLowerCase();
        var found = catalogs[code];
        return found !== undefined ? found : LangEn;
    }

    /* Raw catalog entry: a string, or the form array of a "plural.*" key. */
    function entry(key) {
        var v = catalog.strings[key];
        return v !== undefined ? v : LangEn.strings[key];
    }

    /* Localized text, with {placeholder} filled in from `values`. */
    function t(key, values) {
        var s = entry(key);
        if (s === undefined)
            return key; // untranslated keys stay visible instead of blank
        if (values === undefined)
            return s;
        return s.replace(/\{(\w+)\}/g, function (match, name) {
            return values[name] !== undefined ? values[name] : match;
        });
    }

    /* Whether the device's own catalog carries this key. English is the
     * fallback for everything, so asking `entry()` would answer for the wrong
     * language: a caption that has no form array here must stay the flat
     * string its own catalog wrote, not become an English plural. */
    function has(key) {
        return catalog.strings[key] !== undefined;
    }

    /* Inflected for n: a bare noun ("день" / "дня" / "дней"), or a whole
     * caption where the words after it have to agree too. */
    function plural(key, n) {
        var forms = entry(key);
        if (forms === undefined)
            return "";
        return forms[Math.min(catalog.plural(n), forms.length - 1)];
    }

    readonly property var monthsFull: entry("date.months")
    readonly property var weekdaysShort: entry("date.weekdays")

    /* Day and month in the order the language writes them, with the month in
     * whatever form that order needs (Russian wants the genitive). */
    function fmtDayMonth(d) {
        return t("date.dayMonth", { d: d.getDate(),
                                    month: entry("date.months")[d.getMonth()],
                                    monthGen: entry("date.monthsGen")[d.getMonth()] });
    }

    /* Reading time: "1h 05m" / "1 ч 05 мин" / "12 min" */
    function fmtHM(secs) {
        var s = secs || 0;
        var h = Math.floor(s / 3600);
        var m = Math.floor((s % 3600) / 60);
        if (h > 0)
            return t("time.hm", { h: h, m: (m < 10 ? "0" : "") + m });
        return t("time.m", { m: m });
    }
}
