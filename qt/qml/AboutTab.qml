import QtQuick
import com.pocketbook.controls
import "."

/* Version and the manual update check. The bridge blocks while the firmware
 * talks to GitHub, so the button states here are what tells the user the app
 * is busy rather than stuck. */
Item {
    id: tab

    /* There is no scrolling on this screen — the column has to fit. It grew
     * past the bottom edge once and painted over the navigation bar, so the
     * content is clipped as a backstop and the texts are kept short. */
    clip: true

    readonly property bool busy: updater.state === "connecting"
                                 || updater.state === "checking"
                                 || updater.state === "downloading"
                                 || updater.state === "ready"

    /* Whether the open-book shim is installed. Read on show rather than bound:
     * it lives in two files on the user partition, and nothing signals a
     * change but our own buttons. */
    property bool shimOn: false

    /* Only for the streak line at the top: this screen is otherwise about the
     * app, not about the reading. */
    property var ov: ({})

    /* The card is a door to the year behind the figure. main.qml owns where
     * the screens are, so the tab only says that it was pressed. */
    signal openStreak()

    function refresh() {
        shimOn = shim.installed();
        shimBox.checked = shimOn ? 1 : 0;
        /* Only while the screen is up. This tab is constructed at startup like
         * the other two, and an aggregate on that path costs the launch ~100 ms
         * for a figure nobody is looking at yet; onVisibleChanged brings it in
         * the moment the screen is opened. */
        if (visible)
            ov = stats.overall();
    }

    /* The sentence with a hole where the figure goes: the number is set in the
     * serif at figure size, and the words around it stay body text. Word order
     * differs per language — Turkish opens with the number — so the split is
     * made on a marker rather than on an assumption about where it sits. */
    readonly property var streakParts: {
        var whole = Tr.t("about.streak", {
            n: "\u0001",
            days: Tr.plural("plural.days", tab.ov.streakDays || 0) });
        var cut = whole.indexOf("\u0001");
        if (cut < 0)
            return [whole, ""];
        return [whole.substring(0, cut).trim(),
                whole.substring(cut + 1).trim()];
    }

    Component.onCompleted: refresh()
    onVisibleChanged: if (visible) refresh()

    /* Compact by design: these sit side by side, so they are sized by their
     * label rather than stretched across the page. Pressing inverts the fill —
     * on e-ink that is the one state change that reads instantly. */
    component ActionButton: Rectangle {
        id: btn

        property alias text: label.text
        property bool primary: false
        readonly property bool filled: primary !== area.pressed

        signal clicked()

        width: label.implicitWidth + Global.dp(28)
        height: Global.dp(52)
        radius: GlobalValues.defaultElementBorderRadius
        color: btn.filled && btn.enabled ? GlobalValues.defaultTextColor
                                         : GlobalValues.defaultBackgroundColor
        border.width: btn.filled && btn.enabled
                      ? 0
                      : Math.max(1, Math.round(
                            GlobalValues.defaultSolidSeparatorThickness))
        border.color: btn.enabled ? GlobalValues.defaultTextColor
                                  : GlobalValues.defaultDisabledTextColor

        StyledText {
            id: label

            anchors.centerIn: parent
            styledFont: FontStyles.Body
            color: !btn.enabled ? GlobalValues.defaultDisabledTextColor
                                : (btn.filled ? GlobalValues.defaultBackgroundColor
                                              : GlobalValues.defaultTextColor)
        }

        MouseArea {
            id: area

            anchors.fill: parent
            enabled: btn.enabled
            onClicked: btn.clicked()
        }
    }

    /* The Overview's section heading, repeated here so the two screens read as
     * one app: a caption over a rule, the rule at a quarter of the text
     * colour — never defaultBorderColor, which is black on this firmware. */
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
            height: Math.max(1, Math.round(
                GlobalValues.defaultSolidSeparatorThickness))
            color: GlobalValues.defaultTextColor
            opacity: 0.25
        }
    }

    // The small print: same shape four times over, so it is one component.
    component Note: StyledText {
        width: parent ? parent.width : 0
        wrapMode: Text.Wrap
        styledFont: FontStyles.BodyS
        color: GlobalValues.defaultDisabledTextColor
    }

    Column {
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.topMargin: Global.dp(10)
        anchors.leftMargin: GlobalValues.defaultViewSideMargin
        anchors.rightMargin: GlobalValues.defaultViewSideMargin
        spacing: Global.dp(12)

        /* Days in a row, above everything else on the screen — the one thing
         * here that is about the reading rather than about the app, so it gets
         * the room a headline gets. Hidden below two days: "1 day in a row" is
         * not a run, and this screen still has to fit without scrolling.
         *
         * The wash is the text colour at 5 % alpha, never defaultBorderColor —
         * that one is black on this firmware and would fill the card solid. */
        Rectangle {
            width: parent.width
            height: streakRow.height + 2 * Global.dp(22)
            visible: (tab.ov.streakDays || 0) >= 2
            radius: GlobalValues.defaultElementBorderRadius
            color: Qt.rgba(GlobalValues.defaultTextColor.r,
                           GlobalValues.defaultTextColor.g,
                           GlobalValues.defaultTextColor.b, 0.05)
            border.width: Math.max(1, Math.round(
                GlobalValues.defaultSolidSeparatorThickness))
            border.color: GlobalValues.defaultTextColor

            /* The whole card is the target, not just the words: on e-ink a
             * finger is wider than a line of text, and the frame is what reads
             * as tappable. Pressing dims it, which is the one state change
             * this screen can show instantly. */
            MouseArea {
                id: streakTap

                anchors.fill: parent
                onClicked: tab.openStreak()
            }

            Row {
                id: streakRow

                anchors.centerIn: parent
                opacity: streakTap.pressed ? 0.5 : 1.0
                spacing: Global.dp(7)
                /* The tallest piece is the figure, and the row is only as tall
                 * as that: the card's height is derived from it. */
                height: streakFigure.implicitHeight

                StyledText {
                    anchors.verticalCenter: parent.verticalCenter
                    visible: text !== "" /* Turkish opens with the figure */
                    styledFont: FontStyles.Body
                    color: GlobalValues.defaultTextColor
                    text: tab.streakParts[0]
                }

                StyledText {
                    id: streakFigure

                    anchors.verticalCenter: parent.verticalCenter
                    // The serif the Overview sets its figures in, at the same
                    // size: a number on this screen is the same kind of thing.
                    styledFont: FontStyles.Heading2
                    font.family: "PT Serif"
                    color: GlobalValues.defaultTextColor
                    text: (tab.ov.streakDays || 0) + ""
                }

                StyledText {
                    anchors.verticalCenter: parent.verticalCenter
                    visible: text !== ""
                    styledFont: FontStyles.Body
                    color: GlobalValues.defaultTextColor
                    text: tab.streakParts[1]
                }
            }
        }

        /* A section starts further down than the next line of a paragraph. The
         * spacer sits between two of the Column's own gaps, so dp(4) makes the
         * break ~37 px where the rhythm is ~16. */
        Item { width: 1; height: Global.dp(4) }

        SectionHeading { text: Tr.t("about.section") }

        // The launcher's own glyph beside the name, so the screen introduces
        // itself the way the tile does.
        Row {
            spacing: Global.dp(10)

            NavIcon {
                anchors.verticalCenter: parent.verticalCenter
                width: Global.dp(30)
                height: Global.dp(30)
                kind: "bars"
                active: true
            }

            StyledText {
                anchors.verticalCenter: parent.verticalCenter
                /* The interface font the firmware sets for itself: the name is
                 * a label on a settings screen, not a book title, and the
                 * serif italic it used to wear read as one. A step smaller
                 * than it was, to leave the version room on the same line. */
                styledFont: FontStyles.Heading4
                color: GlobalValues.defaultTextColor
                text: "pocketbook-statistics"
            }

            /* Beside the name rather than on a line of its own: it says which
             * build this is, which is part of what the app is called here, and
             * the button below is then only a button. Written as the tag is —
             * v1.7.0-rc10 — so the screen, the release page and the log all
             * spell the same thing. */
            StyledText {
                anchors.verticalCenter: parent.verticalCenter
                styledFont: FontStyles.BodyS
                color: GlobalValues.defaultTextColor
                opacity: 0.7
                text: "v" + updater.currentVersion
            }
        }

        Row {
            width: parent.width
            spacing: Global.dp(8)

            NavIcon {
                anchors.verticalCenter: parent.verticalCenter
                width: Global.dp(18)
                height: Global.dp(18)
                kind: "github"
                active: true
                opacity: 0.7
            }

            Note {
                anchors.verticalCenter: parent.verticalCenter
                width: parent.width - Global.dp(26)
                wrapMode: Text.NoWrap
                elide: Text.ElideMiddle
                text: "github.com/winst0niuss/pocketbook-statistics"
            }
        }

        ActionButton {
            text: Tr.t("about.check")
            enabled: !tab.busy
            onClicked: updater.check()
        }

        ActionButton {
            visible: updater.state === "available"
            primary: true
            text: Tr.t("about.install", { version: updater.latestVersion })
            enabled: !tab.busy
            onClicked: updater.install()
        }

        StyledText {
            width: parent.width
            wrapMode: Text.Wrap
            styledFont: FontStyles.Body
            color: GlobalValues.defaultTextColor
            visible: text !== ""
            text: {
                switch (updater.state) {
                case "connecting":  return Tr.t("about.connecting");
                case "checking":    return Tr.t("about.checking");
                case "uptodate":    return Tr.t("about.uptodate");
                case "available":   return Tr.t("about.available",
                                                { version: updater.latestVersion });
                case "downloading": return Tr.t("about.downloading");
                case "ready":       return Tr.t("about.ready", { app: Tr.t("app.title") });
                case "error":       return Tr.t(updater.errorKey);
                default:            return "";
                }
            }
        }

        Note {
            visible: updater.state === "error" && updater.errorDetail !== ""
            text: updater.errorDetail
        }

        /* Updating and tracking are separate concerns, so tracking gets a
         * heading of its own rather than a bare rule — and a wider break than
         * the one above the first heading: what sits over this one is a button
         * rather than a line of text, and a control needs the room. dp(16)
         * between two of the Column's gaps comes to ~53 px against the ~37 up
         * there. */
        Item { width: 1; height: Global.dp(16) }

        SectionHeading { text: Tr.t("about.autostart") }

        /* A setting, so it looks like one: the firmware's checkbox with the
         * explanation beside it, rather than a button that has to spell out
         * which way it is about to flip. */
        Row {
            width: parent.width
            spacing: Global.dp(12)

            CheckBox {
                id: shimBox

                anchors.verticalCenter: parent.verticalCenter
                onClicked: {
                    if (tab.shimOn)
                        shim.remove();
                    else
                        shim.install();
                    tab.refresh();
                }
            }

            Column {
                anchors.verticalCenter: parent.verticalCenter
                width: parent.width - shimBox.width - Global.dp(12)
                spacing: Global.dp(4)

                StyledText {
                    width: parent.width
                    wrapMode: Text.Wrap
                    styledFont: FontStyles.Body
                    color: GlobalValues.defaultTextColor
                    text: Tr.t("about.shim")
                }

                Note {
                    text: Tr.t("about.shimHint")
                }
            }
        }

        // What the last attempt managed to do. Worth the screen space while
        // the update path is young: if the firmware takes the process down,
        // this is all that is left of the run.
        // Only when something went wrong: this screen does not scroll, and a
        // log printed under every successful check just falls off the bottom.
        Note {
            visible: updater.state === "error" && updater.diagnostics !== ""
            // The log is up to a dozen lines and this screen does not scroll:
            // capped, because the last lines are the interesting ones and the
            // rest must not push the page over the navigation bar.
            maximumLineCount: 5
            elide: Text.ElideRight
            text: Tr.t("about.log") + "\n" + updater.diagnostics
        }
    }
}
