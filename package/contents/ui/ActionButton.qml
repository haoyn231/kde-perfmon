import QtQuick
import org.kde.kirigami as Kirigami

Rectangle {
    id: button

    property string text
    property bool danger: false
    property bool actionEnabled: true

    signal clicked

    readonly property color tone: button.danger ? Kirigami.Theme.negativeTextColor : Kirigami.Theme.textColor
    readonly property color hoverFill: Qt.rgba(button.tone.r, button.tone.g, button.tone.b, 0.22)
    readonly property color idleFill: Qt.rgba(button.tone.r, button.tone.g, button.tone.b, 0.12)

    implicitWidth: label.implicitWidth + Kirigami.Units.smallSpacing * 3
    implicitHeight: Kirigami.Units.gridUnit * 1.55
    radius: height / 2
    color: !button.actionEnabled ? "transparent" : (hover.hovered ? button.hoverFill : button.idleFill)
    border.width: 1
    border.color: Qt.rgba(button.tone.r, button.tone.g, button.tone.b, button.actionEnabled ? 0.35 : 0.15)
    opacity: button.actionEnabled ? 1 : 0.45

    Text {
        id: label
        anchors.centerIn: parent
        text: button.text
        color: button.tone
        font.pixelSize: Kirigami.Theme.smallFont.pixelSize
        font.weight: Font.DemiBold
    }

    HoverHandler {
        id: hover
        cursorShape: button.actionEnabled ? Qt.PointingHandCursor : Qt.ArrowCursor
    }

    TapHandler {
        enabled: button.actionEnabled
        acceptedButtons: Qt.LeftButton
        onTapped: button.clicked()
    }

    Accessible.role: Accessible.Button
    Accessible.name: button.text
    Accessible.onPressAction: if (button.actionEnabled) button.clicked()
}
