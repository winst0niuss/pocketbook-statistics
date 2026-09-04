import QtQuick
import com.pocketbook.controls

/* Modal panel in the firmware look: title row with X, content as children.
 * Closes via X or a tap on the dimmed background. */
Item {
    id: dlg

    anchors.fill: parent
    visible: false
    z: 10

    property string title: ""
    default property alias content: contentSlot.data

    Rectangle {
        anchors.fill: parent
        color: GlobalValues.defaultTextColor
        opacity: 0.35

        MouseArea {
            anchors.fill: parent
            onClicked: dlg.visible = false
        }
    }

    Rectangle {
        anchors.centerIn: parent
        width: Math.min(GlobalValues.defaultDialogWidth,
                        dlg.width - Global.dp(40))
        height: panelContent.height + 2 * Global.dp(16)
        color: GlobalValues.defaultBackgroundColor
        border.width: GlobalValues.dialogBorderWidth
        border.color: GlobalValues.defaultTextColor

        MouseArea { anchors.fill: parent } // swallows taps inside the panel

        Column {
            id: panelContent

            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.topMargin: Global.dp(16)
            anchors.leftMargin: Global.dp(20)
            anchors.rightMargin: Global.dp(20)
            spacing: Global.dp(16)

            Item {
                width: parent.width
                height: Global.dp(40)

                StyledText {
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.left: parent.left
                    anchors.right: closeBox.left
                    styledFont: FontStyles.Heading4
                    color: GlobalValues.defaultTextColor
                    text: dlg.title
                    elide: Text.ElideRight
                }

                /* The X sits in a box of its own: a letter alone in the
                 * corner reads as part of the title, and on e-ink there is no
                 * hover to say otherwise. The frame is the affordance, the
                 * inverted fill is the press, and the touch target stays
                 * finger-sized around it. */
                Item {
                    id: closeBox
                    anchors.right: parent.right
                    anchors.top: parent.top
                    width: Global.dp(48)
                    height: Global.dp(48)

                    Rectangle {
                        anchors.centerIn: parent
                        width: Global.dp(40)
                        height: Global.dp(40)
                        radius: GlobalValues.defaultElementBorderRadius
                        color: closeArea.pressed ? GlobalValues.defaultTextColor
                                                 : GlobalValues.defaultBackgroundColor
                        border.width: Math.max(1, Math.round(
                            GlobalValues.defaultSolidSeparatorThickness))
                        border.color: GlobalValues.defaultTextColor

                        StyledText {
                            anchors.centerIn: parent
                            styledFont: FontStyles.Body
                            color: closeArea.pressed
                                   ? GlobalValues.defaultBackgroundColor
                                   : GlobalValues.defaultTextColor
                            text: "X"
                        }
                    }

                    MouseArea {
                        id: closeArea

                        anchors.fill: parent
                        onClicked: dlg.visible = false
                    }
                }
            }

            Column {
                id: contentSlot
                width: parent.width
                spacing: Global.dp(12)
            }

            Item { width: 1; height: Global.dp(4) }
        }
    }
}
