import QtQuick
import com.pocketbook.controls
import "."

/* One book, full size: the cover as large as the panel allows, the complete
 * title and the author. The grids elsewhere can only show a thumbnail and a
 * date — for a deleted book they would otherwise show a single letter and
 * nothing else. */
PanelDialog {
    id: dlg

    property var book: ({})
    /* Whatever the calling tab knows about this book: a finish date, time read. */
    property string detail: ""

    function show(b, d) {
        book = b || ({});
        detail = d || "";
        visible = true;
    }

    Row {
        width: parent.width
        spacing: Global.dp(20)

        Item {
            width: Global.dp(120)
            height: Global.dp(180)

            Image {
                id: cover
                anchors.fill: parent
                source: dlg.book.coverUrl || ""
                visible: source != "" && status === Image.Ready
                fillMode: Image.PreserveAspectFit
            }

            Rectangle {
                visible: !cover.visible
                anchors.fill: parent
                color: "transparent"
                border.width: 1
                border.color: GlobalValues.defaultBorderColor

                StyledText {
                    anchors.centerIn: parent
                    styledFont: FontStyles.Heading2
                    color: GlobalValues.defaultDisabledTextColor
                    text: (dlg.book.title || "?").charAt(0)
                }
            }
        }

        Column {
            width: parent.width - Global.dp(140)
            spacing: Global.dp(10)

            StyledText {
                width: parent.width
                styledFont: FontStyles.Heading4
                color: GlobalValues.defaultTextColor
                text: dlg.book.title || ""
                wrapMode: Text.Wrap
                maximumLineCount: 4
                elide: Text.ElideRight
            }

            StyledText {
                width: parent.width
                visible: (dlg.book.author || "") !== ""
                styledFont: FontStyles.BodyL
                color: GlobalValues.defaultDisabledTextColor
                text: dlg.book.author || ""
                wrapMode: Text.Wrap
                maximumLineCount: 2
                elide: Text.ElideRight
            }

            StyledText {
                width: parent.width
                visible: dlg.detail !== ""
                styledFont: FontStyles.BodyS
                color: GlobalValues.defaultTextColor
                text: dlg.detail
            }
        }
    }
}
