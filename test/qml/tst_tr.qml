/* The i18n singleton, on a device set to Russian. The catalogs themselves are
 * checked by tools/check_i18n.mjs; what is tested here is the resolution on top
 * of them — the fallback, the placeholder substitution, and the plural rule
 * that has to agree with the catalog's own forms. */
import QtQuick
import QtTest
import "qrc:/" as App

TestCase {
    name: "Tr"

    function test_translates_into_the_device_language() {
        compare(App.Tr.t("nav.overview"), "Обзор");
    }

    /* A key nothing translates stays visible instead of rendering as a blank
     * space in the middle of a screen. */
    function test_an_unknown_key_renders_as_itself() {
        compare(App.Tr.t("no.such.key"), "no.such.key");
        compare(App.Tr.plural("no.such.plural", 3), "");
        verify(!App.Tr.has("no.such.key"));
        verify(App.Tr.has("nav.overview"));
    }

    function test_placeholders_are_filled_in() {
        compare(App.Tr.t("about.available", { version: "v2.1.0" }),
                "Доступна версия v2.1.0.");
        /* A value nobody passed leaves the placeholder rather than "undefined". */
        compare(App.Tr.t("about.available"), "Доступна версия {version}.");
        compare(App.Tr.t("about.available", { other: "x" }),
                "Доступна версия {version}.");
    }

    function test_plural_data() {
        return [
            { tag: "1 день", n: 1, form: "день" },
            { tag: "2 дня", n: 2, form: "дня" },
            { tag: "5 дней", n: 5, form: "дней" },
            { tag: "11 — not 1", n: 11, form: "дней" },
            { tag: "21 день", n: 21, form: "день" },
            { tag: "22 дня", n: 22, form: "дня" },
            { tag: "111 дней", n: 111, form: "дней" },
            { tag: "0 дней", n: 0, form: "дней" },
        ];
    }

    function test_plural(data) {
        compare(App.Tr.plural("plural.days", data.n), data.form);
    }

    function test_time_is_written_the_way_the_language_writes_it() {
        compare(App.Tr.fmtHM(3900), "1 ч 05 мин");
        compare(App.Tr.fmtHM(3660), "1 ч 01 мин");
        compare(App.Tr.fmtHM(600), "10 мин");
        compare(App.Tr.fmtHM(0), "0 мин");
        compare(App.Tr.fmtHM(undefined), "0 мин");
    }

    /* Russian dates the day before the month, and wants the genitive: the
     * catalog picks {monthGen} where English uses {month}. */
    function test_the_date_takes_the_form_the_word_order_needs() {
        var march9 = new Date(2026, 2, 9);
        compare(App.Tr.fmtDayMonth(march9), "9 марта");
    }

    function test_the_month_and_weekday_lists_are_whole() {
        compare(App.Tr.monthsFull.length, 12);
        compare(App.Tr.weekdaysShort.length, 7);
        compare(App.Tr.weekdaysShort[0], "Пн");
    }
}
