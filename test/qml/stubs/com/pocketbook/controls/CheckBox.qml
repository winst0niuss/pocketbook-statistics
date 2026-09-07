import QtQuick

Item {
    /* The firmware's is an int, and AboutTab assigns 1/0 to it. */
    property int checked: 0
    signal clicked()

    width: 44
    height: 44

    MouseArea {
        anchors.fill: parent
        onClicked: parent.clicked()
    }
}
