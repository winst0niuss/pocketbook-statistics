/* The calendar grid and the month it pages through. */
import QtQuick
import QtTest
import "qrc:/" as App

TestCase {
    id: testCase

    name: "CalendarTab"
    width: 758
    height: 856
    visible: true
    when: windowShown

    Component {
        id: calendar

        App.CalendarTab {
            width: testCase.width
            height: testCase.height
        }
    }

    function test_the_grid_holds_the_whole_month() {
        var tab = createTemporaryObject(calendar, testCase);
        verify(tab, "the tab did not instantiate");

        compare(tab.m.days.length, tab.m.ndays);
        /* Weeks down the grid: the first day sits under its weekday, so the
         * number of rows is what the offset and the length need together. */
        compare(tab.rows, Math.ceil((tab.m.firstWeekday + tab.m.ndays) / 7));
        verify(tab.m.firstWeekday >= 0 && tab.m.firstWeekday <= 6);
    }

    function test_todays_reading_lands_on_todays_cell() {
        var tab = createTemporaryObject(calendar, testCase);
        var today = fixture.today.getDate();

        compare(tab.m.days[today - 1].secs, fixture.todaySecs);
        compare(tab.m.days[today - 1].books.length, 1);
        compare(tab.m.days[today - 1].books[0].title, fixture.title);
        verify(tab.m.readDays >= 1);
    }

    /* Paging is the screen's only navigation, and December has to become
     * January of the next year rather than month 13. */
    function test_paging_crosses_the_turn_of_the_year() {
        var tab = createTemporaryObject(calendar, testCase);
        tab.year = 2026;
        tab.month = 12;

        tab.shiftMonth(1);
        compare(tab.month, 1);
        compare(tab.year, 2027);

        tab.shiftMonth(-1);
        compare(tab.month, 12);
        compare(tab.year, 2026);
    }

    /* February 2028 is a leap year; the grid asks the bridge rather than
     * counting days itself. */
    function test_the_month_length_comes_from_the_bridge() {
        var tab = createTemporaryObject(calendar, testCase);
        tab.year = 2028;
        tab.month = 2;
        tab.refresh();
        compare(tab.m.ndays, 29);
        compare(tab.m.days.length, 29);
    }
}
