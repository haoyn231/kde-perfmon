import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC2
import org.kde.kirigami as Kirigami

import "Format.js" as Format

Item {
    id: row

    required property int pid
    required property string name
    required property string command
    required property real cpu
    required property real memory
    required property real memoryPercent

    property real cpuColumnWidth: 0
    property real memoryColumnWidth: 0
    property real pidColumnWidth: 0

    readonly property bool hasDetails: row.command.length > 0 && row.command !== row.name
    readonly property color hoverColor: Qt.rgba(Kirigami.Theme.textColor.r, Kirigami.Theme.textColor.g, Kirigami.Theme.textColor.b, 0.05)

    implicitHeight: Kirigami.Units.gridUnit * (row.hasDetails ? 2 : 1.4)

    Accessible.name: i18n("%1, CPU %2, memory %3", row.name, Format.percent(row.cpu), Format.formatBytes(row.memory))
    Accessible.description: row.command
    Accessible.role: Accessible.ListItem

    Rectangle {
        anchors.fill: parent
        color: hover.hovered ? row.hoverColor : "transparent"
        radius: Kirigami.Units.smallSpacing
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: Kirigami.Units.smallSpacing
        anchors.rightMargin: Kirigami.Units.smallSpacing
        spacing: Kirigami.Units.smallSpacing

        ColumnLayout {
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            spacing: 0

            Text {
                Layout.fillWidth: true
                text: row.name
                color: Kirigami.Theme.textColor
                font: Kirigami.Theme.defaultFont
                elide: Text.ElideRight
                maximumLineCount: 1
            }

            Text {
                Layout.fillWidth: true
                visible: row.hasDetails
                text: row.command
                color: Kirigami.Theme.disabledTextColor
                font: Kirigami.Theme.smallFont
                elide: Text.ElideRight
                maximumLineCount: 1
            }
        }

        Text {
            Layout.preferredWidth: row.cpuColumnWidth
            Layout.minimumWidth: row.cpuColumnWidth
            text: Format.percent(row.cpu)
            color: Kirigami.Theme.textColor
            font.pixelSize: Kirigami.Theme.smallFont.pixelSize
            font.features: ({ "tnum": 1 })
            horizontalAlignment: Text.AlignRight
            elide: Text.ElideRight
            maximumLineCount: 1
        }

        Text {
            id: memoryText
            Layout.preferredWidth: row.memoryColumnWidth
            Layout.minimumWidth: row.memoryColumnWidth
            text: Format.formatBytes(row.memory)
            color: Kirigami.Theme.textColor
            font.pixelSize: Kirigami.Theme.smallFont.pixelSize
            font.features: ({ "tnum": 1 })
            horizontalAlignment: Text.AlignRight
            elide: Text.ElideRight
            maximumLineCount: 1

            QQC2.ToolTip.visible: memoryHover.hovered
            QQC2.ToolTip.delay: 600
            QQC2.ToolTip.text: i18n("%1 (%2%)", Format.formatBytes(row.memory), Number(row.memoryPercent).toFixed(1))

            HoverHandler {
                id: memoryHover
                acceptedDevices: PointerDevice.Mouse
            }
        }

        Text {
            Layout.preferredWidth: row.pidColumnWidth
            Layout.minimumWidth: row.pidColumnWidth
            text: String(row.pid)
            color: Kirigami.Theme.disabledTextColor
            font.pixelSize: Kirigami.Theme.smallFont.pixelSize
            font.features: ({ "tnum": 1 })
            horizontalAlignment: Text.AlignRight
            elide: Text.ElideRight
            maximumLineCount: 1
        }
    }

    HoverHandler {
        id: hover
    }

}
