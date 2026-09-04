import QtQuick
import com.pocketbook.controls
import "."

Item {
    id: tab

    property int year: new Date().getFullYear()
    property int month: new Date().getMonth() + 1
    property var m: ({ ndays: 30, firstWeekday: 0, days: [], books: [] })

    readonly property var monthNames: Tr.monthsFull

    function refresh() {
        m = stats.month(year, month);
    }

    function shiftMonth(delta) {
        var mo = month + delta;
        var y = year;
        if (mo < 1) { mo = 12; y--; }
        if (mo > 12) { mo = 1; y++; }
        month = mo;
        year = y;
        refresh();
    }

    Component.onCompleted: refresh()
    onVisibleChanged: if (visible) refresh()

    readonly property int rows: Math.ceil((m.firstWeekday + m.ndays) / 7)
    readonly property real sideMargin: GlobalValues.defaultViewSideMargin

    // Month row with large paging zones
    Item {
        id: monthRow

        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: GlobalValues.defaultListItemHeight

        StyledText {
            anchors.verticalCenter: parent.verticalCenter
            anchors.left: parent.left
            anchors.leftMargin: tab.sideMargin
            styledFont: FontStyles.Heading3
            color: GlobalValues.defaultTextColor
            text: "‹"
        }

        Column {
            anchors.centerIn: parent
            spacing: Global.dp(1)

            StyledText {
                anchors.horizontalCenter: parent.horizontalCenter
                styledFont: FontStyles.Heading4
                color: GlobalValues.defaultTextColor
                text: tab.monthNames[tab.month - 1] + " " + tab.year
            }

            StyledText {
                anchors.horizontalCenter: parent.horizontalCenter
                visible: (tab.m.readDays || 0) > 0
                styledFont: FontStyles.BodyXS
                color: GlobalValues.defaultDisabledTextColor
                text: Tr.t("calendar.monthSummary", {
                          n: tab.m.readDays || 0,
                          days: Tr.plural("plural.days", tab.m.readDays || 0),
                          time: Tr.fmtHM(tab.m.totalSecs || 0) })
            }
        }

        StyledText {
            anchors.verticalCenter: parent.verticalCenter
            anchors.right: parent.right
            anchors.rightMargin: tab.sideMargin
            styledFont: FontStyles.Heading3
            color: GlobalValues.defaultTextColor
            text: "›"
        }

        MouseArea {
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            width: parent.width / 3
            onClicked: tab.shiftMonth(-1)
        }

        MouseArea {
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            width: parent.width / 3
            onClicked: tab.shiftMonth(1)
        }
    }

    // Weekday headers
    Row {
        id: weekdayRow

        anchors.top: monthRow.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: tab.sideMargin
        anchors.rightMargin: tab.sideMargin
        height: Global.dp(30)

        Repeater {
            model: Tr.weekdaysShort

            Item {
                required property string modelData
                width: parent.width / 7
                height: parent.height

                StyledText {
                    anchors.centerIn: parent
                    styledFont: FontStyles.BodyS
                    color: GlobalValues.defaultDisabledTextColor
                    text: modelData
                }
            }
        }
    }

    function fmtHM(secs) { return Tr.fmtHM(secs); }

    function openDay(day, entry) {
        var d = new Date(year, month - 1, day);
        // A day whose only entry is a finished book has no measured time.
        dayDialog.title = (entry.secs || 0) > 0
            ? Tr.t("calendar.dayTitle",
                   { date: Tr.fmtDayMonth(d), time: fmtHM(entry.secs) })
            : Tr.fmtDayMonth(d);
        dayDialog.dateFull = Qt.formatDate(d, "dd.MM.yyyy");
        dayDialog.entry = entry;
        dayDialog.visible = true;
    }

    /* Says why days can be blank: before the install date there is no
     * measurement, and a blank cell there means unknown, not "did not read".
     * Finished books still show — the firmware dates those. */
    StyledText {
        id: trackingNote

        visible: (tab.m.trackedFromDay || 0) > 0
                 && (tab.m.trackingSince || "") !== ""
        anchors.bottom: parent.bottom
        anchors.bottomMargin: Global.dp(10)
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: tab.sideMargin
        anchors.rightMargin: tab.sideMargin
        wrapMode: Text.Wrap
        styledFont: FontStyles.BodyS
        color: GlobalValues.defaultDisabledTextColor
        text: Tr.t("calendar.trackingSince", { date: tab.m.trackingSince || "" })
    }

    // Grid: the cover of the most-read book per reading day
    Item {
        id: grid

        anchors.top: weekdayRow.bottom
        anchors.topMargin: Global.dp(6)
        anchors.bottom: trackingNote.visible ? trackingNote.top : parent.bottom
        anchors.bottomMargin: Global.dp(12)
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: tab.sideMargin
        anchors.rightMargin: tab.sideMargin

        readonly property real cw: width / 7
        readonly property real ch: height / tab.rows

        Repeater {
            model: tab.m.ndays

            Rectangle {
                id: cell

                required property int index

                readonly property int day: index + 1
                readonly property int cellIdx: tab.m.firstWeekday + index
                readonly property var entry: (tab.m.days || [])[index]
                                             || ({ secs: 0, books: [] })
                readonly property var topBook: (entry.books || [])[0] || null
                /* Before tracking started nothing was measured, so an empty
                 * day here means "unknown", not "did not read". */
                readonly property bool untracked:
                    cell.day < (tab.m.trackedFromDay || 0)

                /* Rounded, so the grid reads as a row of tiles rather than a
                 * table. That needs the cells apart: butted together, their
                 * corners would leave a pinhole at every crossing, which on
                 * e-ink looks like dirt on the screen. */
                readonly property real inset: Global.dp(1.5)

                x: (cellIdx % 7) * grid.cw + inset
                y: Math.floor(cellIdx / 7) * grid.ch + inset
                width: grid.cw - 2 * inset
                height: grid.ch - 2 * inset
                radius: GlobalValues.defaultElementBorderRadius
                color: "transparent"
                border.width: 1
                border.color: GlobalValues.defaultBorderColor

                StyledText {
                    anchors.top: parent.top
                    anchors.left: parent.left
                    anchors.margins: Global.dp(5)
                    styledFont: FontStyles.BodyS
                    color: cell.untracked ? GlobalValues.defaultDisabledTextColor
                                          : GlobalValues.defaultTextColor
                    text: cell.day
                }

                // Cover: full cell height, out to the right edge; the left
                // keeps room for the day number.
                Item {
                    id: coverBox

                    visible: cell.topBook !== null
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    anchors.right: parent.right
                    anchors.margins: Global.dp(3)
                    anchors.leftMargin: 0
                    width: grid.cw - Global.dp(30)

                    Image {
                        id: coverImg
                        anchors.fill: parent
                        source: cell.topBook ? (cell.topBook.coverUrl || "") : ""
                        visible: source != "" && status === Image.Ready
                        fillMode: Image.PreserveAspectFit
                        horizontalAlignment: Image.AlignRight
                    }

                    Rectangle {
                        visible: !coverImg.visible
                        anchors.fill: parent
                        radius: GlobalValues.defaultElementBorderRadius
                        color: "transparent"
                        border.width: 1
                        border.color: GlobalValues.defaultBorderColor

                        StyledText {
                            anchors.centerIn: parent
                            styledFont: FontStyles.BodyLBold
                            color: GlobalValues.defaultDisabledTextColor
                            text: cell.topBook
                                  ? (cell.topBook.title || "?").charAt(0) : ""
                        }
                    }

                    // "+n" when several books on that day
                    Rectangle {
                        visible: (cell.entry.books || []).length > 1
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        anchors.margins: -Global.dp(4)
                        width: Global.dp(26)
                        height: Global.dp(26)
                        radius: width / 2
                        color: GlobalValues.defaultTextColor

                        StyledText {
                            anchors.centerIn: parent
                            styledFont: FontStyles.BodyXS
                            color: GlobalValues.defaultBackgroundColor
                            text: "+" + ((cell.entry.books || []).length - 1)
                        }
                    }
                }

                MouseArea {
                    anchors.fill: parent
                    // A finished book has no reading time, but still opens.
                    enabled: (cell.entry.books || []).length > 0
                    onClicked: tab.openDay(cell.day, cell.entry)
                }
            }
        }
    }

    // Day dialog: covers side by side, only the reading time per book below;
    // total time is in the title row.
    PanelDialog {
        id: dayDialog

        property var entry: ({ secs: 0, books: [] })
        /* The book card covers this dialog's heading, so it has to repeat
         * which day it is talking about. */
        property string dateFull: ""

        Flow {
            width: parent.width
            spacing: Global.dp(16)

            Repeater {
                model: dayDialog.entry.books || []

                Column {
                    required property var modelData
                    spacing: Global.dp(6)

                    Item {
                        width: Global.dp(100)
                        height: Global.dp(150)

                        MouseArea {
                            anchors.fill: parent
                            z: 1
                            onClicked: bookDialog.show(
                                modelData,
                                (modelData.secs || 0) > 0
                                ? dayDialog.dateFull + "  ·  "
                                  + tab.fmtHM(modelData.secs)
                                : Tr.t("book.finishedOn",
                                       { date: dayDialog.dateFull }))
                        }

                        Image {
                            id: dlgCover
                            anchors.fill: parent
                            source: modelData.coverUrl || ""
                            visible: source != "" && status === Image.Ready
                            fillMode: Image.PreserveAspectFit
                        }

                        Rectangle {
                            visible: !dlgCover.visible
                            anchors.fill: parent
                            color: "transparent"
                            border.width: 1
                            border.color: GlobalValues.defaultBorderColor

                            StyledText {
                                anchors.centerIn: parent
                                styledFont: FontStyles.Heading3
                                color: GlobalValues.defaultDisabledTextColor
                                text: (modelData.title || "?").charAt(0)
                            }
                        }
                    }

                    StyledText {
                        anchors.horizontalCenter: parent.horizontalCenter
                        styledFont: FontStyles.BodyS
                        color: GlobalValues.defaultTextColor
                        text: (modelData.secs || 0) > 0
                              ? tab.fmtHM(modelData.secs)
                              : Tr.t("calendar.finished")
                    }
                }
            }
        }
    }

    // Opened from a cover in the day dialog, so it must sit above it.
    BookDialog { id: bookDialog }
}
