import QtQuick
import QtQuick.Layouts

RowLayout {
    id: indicatorRoot
    property int stateCode: 0 // 0=Stopped, 1=Starting, 2=Running, 3=Error
    property string text: ""
    spacing: 8

    Rectangle {
        width: 10
        height: 10
        radius: 5
        color: {
            switch (indicatorRoot.stateCode) {
            case 2: return "#00e676"; // Running
            case 1: return "#ff9100"; // Starting
            case 3: return "#ff1744"; // Error
            default: return "#64748b"; // Stopped
            }
        }

        SequentialAnimation on opacity {
            running: indicatorRoot.stateCode === 1 || indicatorRoot.stateCode === 2
            loops: Animation.Infinite
            PropertyAnimation { to: 0.35; duration: 650 }
            PropertyAnimation { to: 1.0; duration: 650 }
        }
    }

    Text {
        text: indicatorRoot.text
        color: {
            switch (indicatorRoot.stateCode) {
            case 2: return "#00e676";
            case 1: return "#ff9100";
            case 3: return "#ff1744";
            default: return "#94a3b8";
            }
        }
        font.pixelSize: 11
        font.bold: true
        font.letterSpacing: 1.0
    }
}
