import QtQuick

Item {
    property string title: ""
    signal close()

    height: 88

    Text {
        anchors.centerIn: parent
        text: parent.title.toUpperCase()
    }
}
