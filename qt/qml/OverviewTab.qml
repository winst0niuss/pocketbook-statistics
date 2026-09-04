import QtQuick
import com.pocketbook.controls
import "."

/* Three sections, each a heading over a rule and a row of framed figures: the
 * book in hand, today, and everything ever read. Nothing here is inferred from
 * the library — only what was measured. */
Item {
    id: tab

    property var ov: ({})
    property var book: ({})

    function refresh() {
        ov = stats.overall();
        book = stats.currentBook();
    }

    /* The two all-time figures can be set by hand, for reading this app never
     * saw — before it was installed, or on another reader. What is stored is
     * the difference from the measurement, so today's reading still adds to
     * it, and nothing else on any screen moves. */
    function editTotal(field) {
        editor.field = field;
        editor.title = field === "hours" ? Tr.t("edit.hoursTitle")
                                         : Tr.t("edit.booksTitle");
        editor.value = field === "hours" ? Math.round(ov.totalHours || 0)
                                         : (ov.booksFinished || 0);
        editor.visible = true;
    }

    /* The steps move the figure in the panel and nothing else. Nothing is
     * written until OK: closing by the X or by the backdrop has to leave the
     * card exactly as it was, or a panel opened out of curiosity would change
     * the number. */
    function stepTotal(by) {
        editor.value = Math.max(0, editor.value + by);
    }

    function commitTotal() {
        if (editor.field === "hours")
            stats.setTotalHours(editor.value);
        else
            stats.setBooksFinished(editor.value);
        editor.visible = false;
        refresh();
    }

    /* Cover and title are a way back into the book. currentBook() only fills
     * filePath while the file is still on the device, so a book that was read
     * and then deleted simply is not tappable — there is nothing to open. */
    readonly property bool bookOpenable: (book.filePath || "") !== ""

    function openBook() {
        if (bookOpenable)
            stats.openBook(book.filePath);
    }

    Component.onCompleted: refresh()
    onVisibleChanged: if (visible) refresh()

    readonly property real sideMargin: GlobalValues.defaultViewSideMargin
    readonly property real gap: Global.dp(14)
    readonly property real pad: Global.dp(14)
    readonly property real hairline: Math.max(1, Math.round(
        GlobalValues.defaultSolidSeparatorThickness))
    /* Two cards to a row with one gap between: derive the width rather than
     * place the cards, and the row cannot come out lopsided. */
    readonly property real cardWidth: (width - 2 * sideMargin - gap) / 2

    /* The firmware sets Roboto for its own UI and PT Serif for reading. The
     * figures and the book title use the serif, which reads as "this is about
     * books" rather than "this is a settings screen". */
    readonly property string serif: "PT Serif"

    /* A framed card: hairline outline, faint wash inside. The wash is the text
     * colour at 5 % alpha — never defaultBorderColor, which is black on this
     * firmware and would fill the card solid. Alpha rather than `opacity`,
     * because opacity multiplies onto children and would fade the outline. */
    component Card: Rectangle {
        radius: GlobalValues.defaultElementBorderRadius
        color: Qt.rgba(GlobalValues.defaultTextColor.r,
                       GlobalValues.defaultTextColor.g,
                       GlobalValues.defaultTextColor.b, 0.05)
        border.width: tab.hairline
        border.color: GlobalValues.defaultTextColor
    }

    // A section heading with the rule under it. Every section has one.
    component SectionHeading: Column {
        property alias text: label.text

        width: parent ? parent.width : 0
        spacing: Global.dp(6)

        StyledText {
            id: label

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

    /* One figure over its caption, framed. Both rows use it, so both rows
     * agree on width, height and where the text sits. */
    component StatCard: Item {
        id: card

        property string value: ""
        property string caption: ""
        /* "hours" or "books" on the two all-time cards, empty on the rest: a
         * figure about today is not one you get to overrule. */
        property string field: ""

        width: tab.cardWidth
        height: Global.dp(92)

        Card { anchors.fill: parent }

        /* A long press, not a tap: these cards are read far more often than
         * they are edited, and a tap that opened an editor would be a trap.
         * Pressing dims the card, so the hold reads as doing something. */
        MouseArea {
            id: hold

            anchors.fill: parent
            enabled: card.field !== ""
            onPressAndHold: tab.editTotal(card.field)
        }

        Column {
            anchors.centerIn: parent
            width: parent.width - 2 * Global.dp(10)
            spacing: Global.dp(2)
            opacity: hold.pressed ? 0.5 : 1.0

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

    /* A step, not a keyboard: the firmware's numeric input is not something a
     * Qt scene can raise, and a total is reached in ones and tens anyway. */
    component StepButton: Rectangle {
        id: step

        property alias text: stepLabel.text

        signal clicked()

        height: Global.dp(52)
        radius: GlobalValues.defaultElementBorderRadius
        color: press.pressed ? GlobalValues.defaultTextColor
                             : GlobalValues.defaultBackgroundColor
        border.width: tab.hairline
        border.color: GlobalValues.defaultTextColor

        StyledText {
            id: stepLabel

            anchors.centerIn: parent
            styledFont: FontStyles.Body
            color: press.pressed ? GlobalValues.defaultBackgroundColor
                                 : GlobalValues.defaultTextColor
        }

        MouseArea {
            id: press

            anchors.fill: parent
            onClicked: step.clicked()
        }
    }

    Column {
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.topMargin: Global.dp(14)
        anchors.leftMargin: tab.sideMargin
        anchors.rightMargin: tab.sideMargin
        spacing: Global.dp(14)

        SectionHeading { text: Tr.t("overview.currentBook") }

        // The book in hand
        Item {
            width: parent.width
            /* Whichever is taller: a long title in a 29-language interface can
             * outgrow the cover, and the card must not clip it. */
            height: Math.max(cover.height, bookText.implicitHeight) + 2 * tab.pad
            visible: tab.book.ok === true

            Card { anchors.fill: parent }

            Image {
                id: cover

                anchors.left: parent.left
                anchors.leftMargin: tab.pad
                anchors.verticalCenter: parent.verticalCenter
                source: tab.book.coverUrl || ""
                visible: (tab.book.coverUrl || "") !== ""
                width: Global.dp(104)
                height: Global.dp(156)
                fillMode: Image.PreserveAspectFit
                /* The only feedback a tap gets before the reader takes the
                 * screen: e-ink has no hover and no animation worth the
                 * refresh. */
                opacity: coverTap.pressed ? 0.5 : 1.0

                MouseArea {
                    id: coverTap

                    anchors.fill: parent
                    enabled: tab.bookOpenable
                    onClicked: tab.openBook()
                }
            }

            Column {
                id: bookText

                anchors.left: cover.visible ? cover.right : parent.left
                anchors.leftMargin: tab.pad
                anchors.right: parent.right
                anchors.rightMargin: tab.pad
                anchors.verticalCenter: parent.verticalCenter
                spacing: Global.dp(6)

                StyledText {
                    width: parent.width
                    styledFont: FontStyles.Heading4
                    font.family: tab.serif
                    font.italic: true
                    color: GlobalValues.defaultTextColor
                    text: tab.book.title || ""
                    wrapMode: Text.WordWrap
                    maximumLineCount: 2
                    elide: Text.ElideRight
                    opacity: titleTap.pressed ? 0.5 : 1.0

                    MouseArea {
                        id: titleTap

                        anchors.fill: parent
                        enabled: tab.bookOpenable
                        onClicked: tab.openBook()
                    }
                }

                StyledText {
                    width: parent.width
                    styledFont: FontStyles.BodyS
                    color: GlobalValues.defaultTextColor
                    opacity: 0.7
                    text: tab.book.author || ""
                    elide: Text.ElideRight
                }

                Item { width: 1; height: Global.dp(2) }

                StyledText {
                    styledFont: FontStyles.Body
                    color: GlobalValues.defaultTextColor
                    text: Tr.t("overview.bookProgress", { percent: tab.book.percent || 0 })
                }

                Rectangle {
                    width: parent.width
                    height: Global.dp(12)
                    radius: height / 2
                    color: "transparent"
                    border.width: tab.hairline
                    border.color: GlobalValues.defaultTextColor

                    Rectangle {
                        anchors.left: parent.left
                        anchors.top: parent.top
                        anchors.bottom: parent.bottom
                        anchors.margins: Global.dp(2)
                        radius: height / 2
                        /* A started book always shows a mark: at 1 % the bar
                         * would otherwise look the same as an unopened one,
                         * which is exactly the case worth seeing. */
                        width: (tab.book.percent || 0) > 0
                               ? Math.max(Global.dp(6),
                                          (parent.width - Global.dp(4))
                                          * tab.book.percent / 100)
                               : 0
                        color: GlobalValues.defaultTextColor
                    }
                }

                StyledText {
                    width: parent.width
                    visible: (tab.book.leftSecs || 0) > 0
                    styledFont: FontStyles.BodyS
                    color: GlobalValues.defaultTextColor
                    opacity: 0.7
                    text: Tr.t("overview.left", { time: Tr.fmtHM(tab.book.leftSecs) })
                }
            }
        }

        StyledText {
            visible: tab.book.ok !== true
            styledFont: FontStyles.Body
            color: GlobalValues.defaultTextColor
            opacity: 0.7
            text: Tr.t("overview.noBook")
        }

        SectionHeading { text: Tr.t("overview.today") }

        Row {
            width: parent.width
            spacing: tab.gap

            StatCard {
                readonly property int minutes: Math.round((tab.ov.todaySecs || 0) / 60)
                value: minutes + ""
                caption: Tr.has("plural.minutesToday")
                         ? Tr.plural("plural.minutesToday", minutes)
                         : Tr.t("overview.minutesToday")
            }

            StatCard {
                value: Math.round(tab.ov.pagesPerHour || 0) + ""
                caption: Tr.t("overview.pagesPerHour")
            }
        }

        SectionHeading { text: Tr.t("overview.allBooks") }

        Row {
            width: parent.width
            spacing: tab.gap

            StatCard {
                readonly property int books: tab.ov.booksFinished || 0
                field: "books"
                value: books + ""
                caption: Tr.has("plural.booksFinished")
                         ? Tr.plural("plural.booksFinished", books)
                         : Tr.t("overview.booksFinished")
            }

            StatCard {
                field: "hours"
                /* Whole hours once there are enough to round without losing
                 * anything; a tenth while the figure is still small. */
                value: (tab.ov.totalHours || 0) >= 10
                       ? Math.round(tab.ov.totalHours) + ""
                       : (tab.ov.totalHours || 0).toFixed(1)
                /* Inflected for the rounded figure: the fraction shown below
                 * ten hours has no form of its own to agree with. */
                caption: Tr.has("plural.totalHours")
                         ? Tr.plural("plural.totalHours", Math.round(tab.ov.totalHours || 0))
                         : Tr.t("overview.totalHours")
            }
        }
    }

    /* Held open over the Overview: the figure as it stands, four steps, and
     * the way back to the measurement. Every step writes at once — there is no
     * "save" to forget, and the card behind the panel follows along. */
    PanelDialog {
        id: editor

        property string field: ""
        property int value: 0

        StyledText {
            anchors.horizontalCenter: parent.horizontalCenter
            styledFont: FontStyles.Heading2
            font.family: tab.serif
            color: GlobalValues.defaultTextColor
            text: editor.value + ""
        }

        Row {
            width: parent.width
            spacing: Global.dp(10)

            Repeater {
                model: [-10, -1, 1, 10]

                StepButton {
                    required property int modelData

                    width: (parent.width - 3 * Global.dp(10)) / 4
                    text: (modelData > 0 ? "+" : "\u2212") + Math.abs(modelData)
                    onClicked: tab.stepTotal(modelData)
                }
            }
        }

        StepButton {
            width: parent.width
            text: Tr.t("edit.ok")
            onClicked: tab.commitTotal()
        }
    }
}
