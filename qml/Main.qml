import QtQuick
import QtQuick.Controls

import MyLauncher

Window {
    width: 640
    height: 480

    visible: !backend.mcRunning

    title: qsTr("Hello World")

    Backend {
        id: backend
    }

    ComboBox {
        id: versionSelect
        anchors.centerIn: parent
        editable: true
        model: backend.versions
    }

    Button {
        anchors.top: versionSelect.bottom
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.topMargin: 8

        text: "launch"
        onClicked: backend.launch(versionSelect.currentText)
    }
}
