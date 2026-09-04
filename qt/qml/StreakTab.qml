import QtQuick
import com.pocketbook.controls
import "."

/* The year behind the streak, reached from the line on the About screen: the
 * run you are on, the best one of this year, and every day of it as a square.
 *
 * The grid is split in half — January to June over July to December — because
 * 53 weeks across a 758 px screen would leave a square 11 px wide. Two halves
 * of 27 columns give it 23, which is still a tap target the eye can count. */
Item {
    id: tab

    /* Not `y`: an Item already has one, it is FINAL, and overriding it makes
     * the whole document fail to compile — which reaches the reader as an app
     * that no longer opens. */
    property var yearInfo: ({})

    function refresh() {
        yearInfo = stats.year(new Date().getFullYear());
    }

    Component.onCompleted: refresh()
    onVisibleChanged: if (visible) refresh()

    readonly property real sideMargin: GlobalValues.defaultViewSideMargin
    readonly property real gap: Global.dp(14)
    /* Derived, never placed: two cards and one gap cannot come out lopsided
     * this way. The Overview sizes its cards by the same rule. */
    readonly property real cardWidth: (width - 2 * sideMargin - gap) / 2
    readonly property real hairline: Math.max(1, Math.round(
        GlobalValues.defaultSolidSeparatorThickness))
    /* The serif the Overview sets its figures in: a number is a number
     * wherever it stands. */
    readonly property string serif: "PT Serif"
    readonly property int ndays: tab.yearInfo.ndays || 365
    readonly property var days: tab.yearInfo.days || []
    /* Days before this index were never measured, so they are not "not read".
     * 0 means the whole year was. */
    readonly property int trackedFrom: tab.yearInfo.trackedFrom || 0

    /* Day of the year (0-based) the given month starts on. UTC arithmetic on
     * purpose: subtracting two local dates across a daylight-saving change is
     * off by an hour, and the floor then lands a day early. */
    function monthStart(mon) {
        var yr = tab.yearInfo.year || new Date().getFullYear();
        return Math.round((Date.UTC(yr, mon, 1) - Date.UTC(yr, 0, 1)) / 86400000);
    }

    /* The Overview's card, so the two screens read as one app: a hairline
     * frame over a wash of the text colour at 5 % alpha — never
     * defaultBorderColor, which is black on this firmware and would fill the
     * card solid. */
    component Figure: Item {
        property string value: ""
        property string caption: ""

        width: tab.cardWidth
        height: Global.dp(92)

        Rectangle {
            anchors.fill: parent
            radius: GlobalValues.defaultElementBorderRadius
            color: Qt.rgba(GlobalValues.defaultTextColor.r,
                           GlobalValues.defaultTextColor.g,
                           GlobalValues.defaultTextColor.b, 0.05)
            border.width: tab.hairline
            border.color: GlobalValues.defaultTextColor
        }

        Column {
            anchors.centerIn: parent
            width: parent.width - 2 * Global.dp(10)
            spacing: Global.dp(2)

            StyledText {
                anchors.horizontalCenter: parent.horizontalCenter
                styledFont: FontStyles.Heading2
                font.family: tab.serif
                color: GlobalValues.defaultTextColor
                text: value
            }

            StyledText {
                width: parent.width
                horizontalAlignment: Text.AlignHCenter
                styledFont: FontStyles.BodyS
                color: GlobalValues.defaultTextColor
                opacity: 0.7
                text: caption
                wrapMode: Text.WordWrap
                maximumLineCount: 2
                elide: Text.ElideRight
            }
        }
    }

    /* One square: read or not, and a third case that is easy to get wrong —
     * a day before tracking began is unknown rather than unread, so it is an
     * empty outline instead of a filled cell.
     *
     * Read is 65 % of the text colour rather than the colour itself, because a
     * wall of solid black reads as a blot on e-ink — but a grey fill on its own
     * reads as a faded print, so every square carries an edge a shade darker
     * than what it holds. The outline is also what keeps the three states
     * apart once two of them are pale. */
    component Day: Rectangle {
        /* Not `state`: every Item already has one, and it is a string. */
        property int mark: 0
        property bool tracked: true

        readonly property real ink: !tracked ? 0.0 : (mark === 0 ? 0.10 : 0.65)
        readonly property real edge: !tracked ? 0.15 : (mark === 0 ? 0.35 : 0.85)

        radius: Math.round(width / 5)
        color: Qt.rgba(GlobalValues.defaultTextColor.r,
                       GlobalValues.defaultTextColor.g,
                       GlobalValues.defaultTextColor.b, ink)
        border.width: tab.hairline
        border.color: Qt.rgba(GlobalValues.defaultTextColor.r,
                              GlobalValues.defaultTextColor.g,
                              GlobalValues.defaultTextColor.b, edge)
    }

    /* Half a year: weekdays down, weeks across, month names underneath. The
     * cell size is derived from the width rather than fixed — the squares must
     * end where the text does, on whatever screen this runs. */
    component HalfYear: Item {
        property int fromDay: 0    // first day of the year, 0-based
        property int toDay: 0      // one past the last
        property int fromMonth: 0
        property int toMonth: 0

        readonly property int firstWeekday:
            ((tab.yearInfo.firstWeekday || 0) + fromDay) % 7
        readonly property int columns:
            Math.ceil((toDay - fromDay + firstWeekday) / 7)
        readonly property real gap: Math.max(1, Math.round(Global.dp(2)))
        /* Never negative: the width is 0 until the column has one, and a
         * Rectangle sized from that would complain once per square. */
        readonly property real cell:
            columns > 0
            ? Math.max(0, Math.floor((width - (columns - 1) * gap) / columns))
            : 0
        readonly property real step: cell + gap

        width: parent ? parent.width : 0
        height: 7 * cell + 6 * gap + Global.dp(6) + monthLabels.height

        Repeater {
            model: Math.max(0, toDay - fromDay)

            Day {
                required property int index

                readonly property int dayIndex: fromDay + index
                readonly property int slot: index + firstWeekday

                x: Math.floor(slot / 7) * step
                y: (slot % 7) * step
                width: cell
                height: cell
                mark: tab.days[dayIndex] || 0
                tracked: dayIndex >= tab.trackedFrom
            }
        }

        Item {
            id: monthLabels

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.topMargin: 7 * cell + 6 * gap + Global.dp(6)
            height: Global.dp(22)

            Repeater {
                model: Math.max(0, toMonth - fromMonth + 1)

                StyledText {
                    required property int index

                    readonly property int start:
                        tab.monthStart(fromMonth + index) - fromDay + firstWeekday

                    x: Math.floor(start / 7) * step
                    styledFont: FontStyles.BodyXS
                    color: GlobalValues.defaultTextColor
                    opacity: 0.6
                    text: Tr.monthsFull[fromMonth + index].substring(0, 3)
                }
            }
        }
    }

    /* The caption over its rule, as both other screens head a section. */
    component SectionHeading: Column {
        property alias text: heading.text

        width: parent ? parent.width : 0
        spacing: Global.dp(6)

        StyledText {
            id: heading

            styledFont: FontStyles.Caption1
            color: GlobalValues.defaultTextColor
            opacity: 0.7
        }

        Rectangle {
            width: parent.width
            height: tab.hairline
            color: GlobalValues.defaultTextColor
            opacity: 0.25
        }
    }

    // The legend, in the order the states are worth reading: the ordinary day,
    // the day that counts, the day a book ended.
    component LegendItem: Row {
        id: legend

        property alias text: caption.text
        property int mark: 0

        spacing: Global.dp(6)

        Day {
            anchors.verticalCenter: parent.verticalCenter
            width: Global.dp(16)
            height: Global.dp(16)
            mark: legend.mark
        }

        StyledText {
            id: caption

            anchors.verticalCenter: parent.verticalCenter
            styledFont: FontStyles.BodyXS
            color: GlobalValues.defaultTextColor
            opacity: 0.7
        }
    }

    Column {
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.topMargin: Global.dp(14)
        anchors.leftMargin: tab.sideMargin
        anchors.rightMargin: tab.sideMargin
        spacing: Global.dp(12)

        Row {
            width: parent.width
            spacing: tab.gap

            Figure {
                value: (tab.yearInfo.current || 0) + ""
                caption: Tr.t("streak.currentCaption")
            }

            Figure {
                value: (tab.yearInfo.best || 0) + ""
                caption: Tr.t("streak.bestCaption", { year: tab.yearInfo.year || 0 })
            }
        }

        /* A section starts further down than the next line of a paragraph, and
         * a spacer sits between two of the Column's own gaps: dp(8) makes the
         * break ~43 px where the rhythm is ~16. There is room for it — the
         * page ends ~120 px above the navigation bar. */
        Item { width: 1; height: Global.dp(8) }

        SectionHeading {
            text: Tr.t("streak.readDaysCaption", {
                      n: tab.yearInfo.readDays || 0,
                      days: Tr.plural("plural.days", tab.yearInfo.readDays || 0),
                      year: tab.yearInfo.year || 0 })
        }

        HalfYear {
            fromDay: 0
            toDay: tab.monthStart(6)
            fromMonth: 0
            toMonth: 5
        }

        HalfYear {
            fromDay: tab.monthStart(6)
            toDay: tab.ndays
            fromMonth: 6
            toMonth: 11
        }

        /* A Flow, not a Row: two legends still run long in some languages, and
         * wrapping is better than a word running off the edge. */
        Flow {
            width: parent.width
            spacing: Global.dp(16)

            LegendItem {
                mark: 0
                text: Tr.t("streak.notRead")
            }

            LegendItem {
                mark: 1
                text: Tr.t("streak.read")
            }
        }

        // A screen that can show a blank day has to say why it is blank.
        StyledText {
            width: parent.width
            visible: tab.trackedFrom > 0 && (tab.yearInfo.trackingSince || "") !== ""
            wrapMode: Text.Wrap
            styledFont: FontStyles.BodyXS
            color: GlobalValues.defaultDisabledTextColor
            text: Tr.t("calendar.trackingSince", { date: tab.yearInfo.trackingSince || "" })
        }
    }
}
