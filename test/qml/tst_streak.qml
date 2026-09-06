/* The year as a grid of squares, split in half because 53 weeks across 758 px
 * leaves an 11 px square. */
import QtQuick
import QtTest
import "qrc:/" as App

TestCase {
    id: testCase

    name: "StreakTab"
    width: 758
    height: 856
    visible: true
    when: windowShown

    Component {
        id: streak

        App.StreakTab {
            width: testCase.width
            height: testCase.height
        }
    }

    function test_the_year_arrives_whole() {
        var tab = createTemporaryObject(streak, testCase);
        verify(tab, "the tab did not instantiate");

        var days = new Date(new Date().getFullYear(), 11, 31);
        compare(tab.ndays, days.getFullYear() % 4 === 0
                && (days.getFullYear() % 100 !== 0 || days.getFullYear() % 400 === 0)
                ? 366 : 365);
        compare(tab.days.length, tab.ndays);
        compare(tab.yearInfo.current, fixture.streakDays);
        verify(tab.yearInfo.best >= fixture.streakDays);
        /* The whole year was measured in this fixture. */
        compare(tab.trackedFrom, 0);
    }

    /* Not `y`: an Item already has one, it is FINAL, and overriding it makes
     * the document fail to compile — which reaches the reader as an app that
     * no longer opens. */
    function test_the_year_property_is_not_called_y() {
        var tab = createTemporaryObject(streak, testCase);
        compare(tab.y, 0);
        verify(tab.yearInfo !== undefined);
    }

    /* Day of the year each month starts on. UTC arithmetic on purpose: two
     * local dates subtracted across a daylight-saving change are an hour
     * short, and the floor then lands a day early. */
    function test_month_starts_are_exact_data() {
        return [
            { tag: "January", mon: 0, day: 0 },
            { tag: "February", mon: 1, day: 31 },
            { tag: "July", mon: 6, day: 181 },
            { tag: "December", mon: 11, day: 334 },
        ];
    }

    function test_month_starts_are_exact(data) {
        var tab = createTemporaryObject(streak, testCase);
        var year = tab.yearInfo.year;
        var leap = year % 4 === 0 && (year % 100 !== 0 || year % 400 === 0);
        var expected = data.day + (leap && data.mon > 1 ? 1 : 0);
        compare(tab.monthStart(data.mon), expected);
    }

    function test_two_cards_and_a_gap_fit_the_screen_exactly() {
        var tab = createTemporaryObject(streak, testCase);
        compare(2 * tab.cardWidth + tab.gap + 2 * tab.sideMargin, tab.width);
    }
}
