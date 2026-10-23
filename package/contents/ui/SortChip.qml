import QtQuick
import org.kde.kirigami as Kirigami

Rectangle {
    id: chip

    property string text
    property bool active: false
    signal clicked

    readonly property color idleColor: Qt.rgba(Kirigami.Theme.textColor.r, Kirigami.Theme.textColor.g, Kirigami.Theme.textColor.b, 0.06)
    readonly property color hoverColor: Qt.rgba(Kirigami.Theme.textColor.r, Kirigami.Theme.textColor.g, Kirigami.Theme.textColor.b, 0.14)

    implicitWidth: label.implicitWidth + Kirigami.Units.smallSpacing * 3
    implicitHeight: Kirigami.Units.gridUnit * 1.55
    radius: height / 2

    color: chip.active ? Kirigami.Theme.highlightColor : (hover.hovered ? chip.hoverColor : chip.idleColor)
    border.width: chip.active ? 0 : 1
    border.color: Qt.rgba(Kirigami.Theme.textColor.r, Kirigami.Theme.textColor.g, Kirigami.Theme.textColor.b, 0.12)

    Behavior on color {
        ColorAnimation { duration: Kirigami.Units.shortDuration }
    }

    Text {
        id: label
        anchors.centerIn: parent
        text: chip.text
        color: chip.active ? Kirigami.Theme.highlightedTextColor : Kirigami.Theme.textColor
        font.pixelSize: Kirigami.Theme.smallFont.pixelSize
        font.weight: chip.active ? Font.DemiBold : Font.Normal
    }

    HoverHandler {
        id: hover
        cursorShape: Qt.PointingHandCursor
    }

    TapHandler {
        acceptedButtons: Qt.LeftButton
        onTapped: chip.clicked()
    }

    Accessible.role: Accessible.Button
    Accessible.name: chip.text
    Accessible.checkable: true
    Accessible.checked: chip.active
    Accessible.onPressAction: chip.clicked()
}
