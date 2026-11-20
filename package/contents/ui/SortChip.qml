import QtQuick
import org.kde.kirigami as Kirigami

Rectangle {
    id: chip

    property string text
    property bool active: false
    property bool descending: false
    signal clicked

    readonly property color idleColor: Qt.rgba(Kirigami.Theme.textColor.r, Kirigami.Theme.textColor.g, Kirigami.Theme.textColor.b, 0.06)
    readonly property color hoverColor: Qt.rgba(Kirigami.Theme.textColor.r, Kirigami.Theme.textColor.g, Kirigami.Theme.textColor.b, 0.14)

    implicitWidth: content.implicitWidth + Kirigami.Units.smallSpacing * 3
    implicitHeight: Kirigami.Units.gridUnit * 1.55
    radius: height / 2

    color: chip.active ? Kirigami.Theme.highlightColor : (hover.hovered ? chip.hoverColor : chip.idleColor)
    border.width: chip.active ? 0 : 1
    border.color: Qt.rgba(Kirigami.Theme.textColor.r, Kirigami.Theme.textColor.g, Kirigami.Theme.textColor.b, 0.12)

    Behavior on color {
        ColorAnimation { duration: Kirigami.Units.shortDuration }
    }

    Row {
        id: content
        anchors.centerIn: parent
        spacing: Kirigami.Units.smallSpacing / 2

        Text {
            id: label
            anchors.verticalCenter: parent.verticalCenter
            text: chip.text
            color: chip.active ? Kirigami.Theme.highlightedTextColor : Kirigami.Theme.textColor
            font.pixelSize: Kirigami.Theme.smallFont.pixelSize
            font.weight: chip.active ? Font.DemiBold : Font.Normal
        }

        // 方向箭头跟着当前排序列走。非激活时只把宽度占住（透明），
        // 这样切换 CPU / Memory 时整行不会因为多出一个箭头而抖动。
        Text {
            id: arrow
            anchors.verticalCenter: parent.verticalCenter
            text: chip.descending ? "↓" : "↑"
            opacity: chip.active ? 1 : 0
            color: Kirigami.Theme.highlightedTextColor
            font.pixelSize: label.font.pixelSize
            font.bold: true

            Behavior on opacity {
                NumberAnimation { duration: Kirigami.Units.shortDuration }
            }
        }
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
    Accessible.name: chip.active
        ? (chip.descending ? i18n("%1, descending", chip.text) : i18n("%1, ascending", chip.text))
        : chip.text
    Accessible.checkable: true
    Accessible.checked: chip.active
    Accessible.onPressAction: chip.clicked()
}
