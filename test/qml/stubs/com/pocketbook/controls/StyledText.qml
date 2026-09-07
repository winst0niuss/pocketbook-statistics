import QtQuick

Text {
    property var styledFont: ({ pixelSize: 24 })

    font.pixelSize: styledFont && styledFont.pixelSize ? styledFont.pixelSize : 24
    font.bold: styledFont && styledFont.bold ? true : false
    renderType: Text.NativeRendering
}
