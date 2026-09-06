/* The Overview against a seeded database: what it reads out of the bridge, the
 * card arithmetic that has to fit the screen, and the step dialog that is the
 * one place in the app where a user changes a number. */
import QtQuick
import QtTest
import "qrc:/" as App

TestCase {
    id: testCase

    name: "OverviewTab"
    width: 758
    height: 856
    visible: true
    when: windowShown

    Component {
        id: overview

        App.OverviewTab {
            width: testCase.width
            height: testCase.height
        }
    }

    function test_it_shows_what_the_bridge_measured() {
        var tab = createTemporaryObject(overview, testCase);
        verify(tab, "the tab did not instantiate");

        compare(tab.ov.todaySecs, fixture.todaySecs);
        compare(tab.ov.totalHours, fixture.totalHours);
        compare(Math.round(tab.ov.pagesPerHour), fixture.pagesPerHour);
        compare(tab.ov.streakDays, fixture.streakDays);
        compare(tab.book.ok, true);
        compare(tab.book.title, fixture.title);
        compare(tab.book.percent, fixture.percent);
    }

    /* A row of two cards and one gap between them, inside the side margins.
     * The cards are sized from the width rather than placed, so the row cannot
     * come out lopsided — and a spacer added beside a Row that already had
     * spacing is what once pushed a figures row 21 px past the screen edge. */
    function test_two_cards_and_a_gap_fit_the_screen_exactly() {
        var tab = createTemporaryObject(overview, testCase);
        compare(2 * tab.cardWidth + tab.gap + 2 * tab.sideMargin, tab.width);
        verify(tab.cardWidth > 0);
    }

    /* The card is a way back into the book, and only where there is still a
     * file to open: this fixture's book has no files row. */
    function test_a_book_that_is_not_on_the_device_is_not_tappable() {
        var tab = createTemporaryObject(overview, testCase);
        compare(tab.book.filePath, "");
        compare(tab.bookOpenable, false);
        tab.openBook(); /* must be a no-op rather than an error */
    }

    /* The steps move the figure in the panel and nothing else; only OK writes.
     * A panel opened out of curiosity must leave the card exactly as it was. */
    function test_the_step_dialog_writes_only_on_ok() {
        var tab = createTemporaryObject(overview, testCase);
        var measured = tab.ov.totalHours;

        tab.editTotal("hours");
        tab.stepTotal(5);
        tab.refresh();
        compare(tab.ov.totalHours, measured, "nothing is written before OK");

        tab.editTotal("hours");
        tab.stepTotal(5);
        tab.commitTotal();
        compare(tab.ov.totalHours, Math.round(measured) + 5);

        /* And back to the measurement, for the tests after this one. */
        stats.setTotalHours(-1);
        tab.refresh();
        compare(tab.ov.totalHours, measured);
    }

    function test_the_step_dialog_never_goes_below_zero() {
        var tab = createTemporaryObject(overview, testCase);
        tab.editTotal("books");
        tab.stepTotal(-50);
        tab.commitTotal();
        compare(tab.ov.booksFinished, 0);

        stats.setBooksFinished(-1);
        tab.refresh();
        compare(tab.ov.booksFinished, 0);
    }
}
