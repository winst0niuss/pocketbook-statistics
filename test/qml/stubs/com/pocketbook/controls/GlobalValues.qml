pragma Singleton
import QtQuick

QtObject {
    readonly property color defaultTextColor: "#000000"
    readonly property color defaultDisabledTextColor: "#808080"
    readonly property color defaultBackgroundColor: "#ffffff"
    /* Black on this firmware — a line colour, not a tint. Filling a shape with
     * it and drawing on top produces a black rectangle, which is how the
     * progress bar and a section heading both disappeared. */
    readonly property color defaultBorderColor: "#000000"
    readonly property real defaultElementBorderRadius: 4
    readonly property real defaultSolidSeparatorThickness: 1
    readonly property real defaultViewSideMargin: 24
    readonly property real defaultListItemHeight: 72
    readonly property real defaultDialogWidth: 600
    readonly property real dialogBorderWidth: 2
}
