pragma Singleton
import QtQuick

QtObject {
    /* Global.dp(1) measures ~1.32 px on a 758x1024 PB629. Every margin in the
     * app is a multiple of this, so a layout test that used 1.0 would not be
     * checking the device's arithmetic. */
    readonly property real ratio: 1.32

    function dp(n) {
        return n * ratio;
    }
}
