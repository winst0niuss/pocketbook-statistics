/* The window itself. An empty scene is the failure this catches: a property
 * clash or a duplicated id stops the root component being created, main.cpp
 * exits, and the app simply stops opening with no message anywhere. */
import QtQuick
import QtTest
import "qrc:/" as App

TestCase {
    id: testCase

    name: "Shell"
    when: windowShown

    Component {
        id: about

        App.AboutTab {
            width: 758
            height: 856
        }
    }

    /* main.qml is loaded by URL rather than as a type: its name is lowercase,
     * which makes it a file the engine loads and not a component to declare. */
    function test_the_scene_instantiates() {
        var component = Qt.createComponent("qrc:/main.qml");
        compare(component.status, Component.Ready, component.errorString());

        var window = component.createObject(null);
        verify(window, "main.qml produced no object — the app would not open");
        compare(window.title, App.Tr.t("app.title"));
        compare(window.width, 758);
        compare(window.height, 1024 - 80);
        window.destroy();
    }

    /* The About screen is where the update button and the autostart switch
     * live; both read their state from the bridges rather than from a
     * binding. */
    function test_about_reads_its_state_from_the_bridges() {
        var tab = createTemporaryObject(about, testCase);
        verify(tab);
        compare(tab.busy, false);
        tab.refresh();
        compare(tab.shimOn, shim.installed());
        compare(updater.state, "idle");
        verify(updater.currentVersion.length > 0);
    }
}
