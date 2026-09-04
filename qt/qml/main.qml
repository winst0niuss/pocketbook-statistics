import QtQuick
import QtQuick.Window
import com.pocketbook.controls
import "."

Window {
    id: root

    visible: true
    width: screenW
    height: screenH - panelH
    color: GlobalValues.defaultBackgroundColor
    // The header says what the app is, in the reader's language;
    // AppHeader upper-cases it itself.
    title: Tr.t("app.title")

    AppHeader {
        id: appHeader

        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right

        title: root.title
        onClose: Qt.quit()
    }

    /* Info sits in the header, opposite the firmware's home button: it is a
     * detour from the app rather than one of its screens, and the bottom bar
     * is for the screens. Drawn over AppHeader, which leaves its right side
     * empty, and matched to the home button beside it — same height share of
     * the bar, full-strength ink, and a line as thin as the firmware's own
     * icons rather than the heavier weight the bottom bar uses. */
    Item {
        id: infoButton

        /* Measured off the firmware's home button on a PB629: a ~30 dp glyph
         * drawn with a hairline, sitting 20 dp from its edge of the screen.
         * NavIcon keeps a tenth of its box as padding, so the box is bigger
         * than the glyph and the margin gives that padding back — otherwise
         * the "i" would sit further in than the house opposite it. */
        readonly property real glyph: Global.dp(30)

        anchors.right: appHeader.right
        anchors.rightMargin: Math.round(Global.dp(20) - (width - glyph) / 2)
        anchors.verticalCenter: appHeader.verticalCenter
        width: Math.round(glyph / 0.8)
        height: width
        z: 1

        NavIcon {
            anchors.fill: parent
            kind: "info"
            // A header icon is never "inactive": the home button is solid
            // black whatever screen you are on.
            opacity: 1.0
            // The house is drawn with a 1.5 dp line at any size; a share of the
            // icon would thicken with it.
            strokeRatio: Global.dp(1.5) / Math.min(width, height)
        }

        MouseArea {
            // The target stays finger-sized even though the glyph is not.
            anchors.fill: parent
            anchors.margins: -Global.dp(10)
            // Tapping it again goes back where you were, so the button is a
            // toggle rather than a one-way door.
            onClicked: nav.current = nav.current === nav.infoIndex
                       ? nav.lastScreen : nav.infoIndex
        }
    }

    /* Navigation along the bottom edge, icon over label — the screens only;
     * info lives in the header. Equal cells are honest here: the icons are all
     * the same width, unlike the word-sized tabs this replaced. */
    Item {
        id: nav

        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        height: Global.dp(70)

        property int current: 0
        // Where the info button returns to.
        property int lastScreen: 0
        // Only a screen from the bar is worth returning to: remembering the
        // info detours would make the header button bounce between them.
        onCurrentChanged: if (current < infoIndex) lastScreen = current

        readonly property var labels: [Tr.t("nav.overview"), Tr.t("nav.calendar")]
        readonly property var icons: ["home", "calendar"]
        readonly property int infoIndex: labels.length
        /* Reached only from the line on the About screen, and left the same
         * way — the bottom bar is for the screens, and this is a detour off a
         * detour. */
        readonly property int streakIndex: infoIndex + 1

        Rectangle {
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            height: Math.max(1, Math.round(GlobalValues.defaultSolidSeparatorThickness))
            color: GlobalValues.defaultBorderColor
        }

        Row {
            anchors.fill: parent
            anchors.topMargin: Global.dp(6)

            Repeater {
                model: nav.labels.length

                Item {
                    id: navCell

                    required property int index

                    readonly property bool active: index === nav.current

                    width: nav.width / nav.labels.length
                    height: nav.height

                    Column {
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.top: parent.top
                        spacing: Global.dp(2)

                        NavIcon {
                            anchors.horizontalCenter: parent.horizontalCenter
                            width: Global.dp(32)
                            height: Global.dp(32)
                            kind: nav.icons[navCell.index]
                            active: navCell.active
                        }

                        StyledText {
                            anchors.horizontalCenter: parent.horizontalCenter
                            // One size only: a bold variant would change the
                            // label's width and shuffle the row on every tap.
                            styledFont: FontStyles.BodyXS
                            color: GlobalValues.defaultTextColor
                            opacity: navCell.active ? 1.0 : 0.6
                            text: nav.labels[navCell.index]
                        }
                    }

                    MouseArea {
                        anchors.fill: parent
                        onClicked: nav.current = index
                    }
                }
            }
        }
    }

    /* Coming back from the reader is neither of the two moments a tab refreshes
     * on — it is not created again, and it never stopped being "visible", it
     * was just behind the book. So the figures stayed at whatever they were
     * when the app was last looked at, and only switching tabs brought them
     * up to date. Refresh when the app becomes active again, and once a minute
     * while it is: reading the aggregates costs under 100 ms, and the screen
     * only repaints if a number actually changed. */
    function refreshCurrent() {
        if (nav.current === 0)
            overviewTab.refresh();
        else if (nav.current === 1)
            calendarTab.refresh();
    }

    Connections {
        target: Qt.application

        function onStateChanged() {
            if (Qt.application.state === Qt.ApplicationActive)
                root.refreshCurrent();
        }
    }

    Timer {
        interval: 60000
        // Only while the app is in front: behind a book this would query the
        // database once a minute for a screen nobody is looking at, competing
        // with the daemon for the same file. If the platform never reports a
        // state change the value stays Active, and the timer keeps running.
        running: Qt.application.state === Qt.ApplicationActive
        repeat: true
        onTriggered: root.refreshCurrent()
    }

    FocusScope {
        anchors.top: appHeader.bottom
        anchors.bottom: nav.top
        anchors.left: parent.left
        anchors.right: parent.right
        focus: true

        Keys.onPressed: function (event) {
            if (event.key === Qt.Key_Back || event.key === Qt.Key_Escape
                    || event.key === Qt.Key_Home) {
                event.accepted = true;
                Qt.quit();
            }
        }

        OverviewTab {
            id: overviewTab

            anchors.fill: parent
            visible: nav.current === 0
        }

        CalendarTab {
            id: calendarTab

            anchors.fill: parent
            visible: nav.current === 1
        }

        AboutTab {
            anchors.fill: parent
            visible: nav.current === nav.infoIndex
            onOpenStreak: nav.current = nav.streakIndex
        }

        /* Built when it is first opened, never at startup. Three tabs are
         * already constructed before the first frame and that is most of the
         * ~3.2 s launch; a year of squares and the query behind them would
         * land on that path for a screen most sessions never open. Leaving it
         * drops the page again, which costs nothing — it reads the year afresh
         * every time it is shown anyway. */
        Component {
            id: streakPage

            StreakTab { }
        }

        Loader {
            anchors.fill: parent
            active: nav.current === nav.streakIndex
            visible: active
            sourceComponent: streakPage
        }
    }
}
