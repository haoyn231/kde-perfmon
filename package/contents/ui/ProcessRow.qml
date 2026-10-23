import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC2
import org.kde.kirigami as Kirigami

import "Format.js" as Format

Item {
    id: row

    required property int pid
    required property int ppid
    required property string name
    required property string command
    required property real cpu
    required property real memory
    required property real memoryPercent

    property real cpuColumnWidth: 0
    property real memoryColumnWidth: 0
    property real pidColumnWidth: 0

    property bool expanded: false
    property bool confirmPending: false
    property bool confirmForce: false
    property string actionError: ""

    signal toggleRequested
    signal processActionRequested(bool force)
    signal actionConfirmed
    signal actionCancelled

    readonly property bool hasDetails: row.command.length > 0 && row.command !== row.name
    readonly property color hoverColor: Qt.rgba(Kirigami.Theme.textColor.r, Kirigami.Theme.textColor.g, Kirigami.Theme.textColor.b, 0.05)
    readonly property color detailsColor: Qt.rgba(Kirigami.Theme.textColor.r, Kirigami.Theme.textColor.g, Kirigami.Theme.textColor.b, 0.04)

    implicitHeight: mainRow.implicitHeight
        + (row.expanded
            ? details.implicitHeight + Kirigami.Units.smallSpacing * 2
            : 0)

    Accessible.name: i18n("%1, CPU %2, memory %3", row.name, Format.percent(row.cpu), Format.formatBytes(row.memory))
    Accessible.description: row.command
    Accessible.role: Accessible.ListItem
    Accessible.onPressAction: row.toggleRequested()

    Rectangle {
        anchors.fill: parent
        color: hover.hovered ? row.hoverColor : "transparent"
        radius: Kirigami.Units.smallSpacing
    }

    Rectangle {
        anchors.fill: parent
        visible: row.expanded
        color: row.detailsColor
        radius: Kirigami.Units.smallSpacing
    }

    MouseArea {
        anchors.fill: mainRow
        cursorShape: Qt.PointingHandCursor
        onClicked: row.toggleRequested()
    }

    RowLayout {
        id: mainRow

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.leftMargin: Kirigami.Units.smallSpacing
        anchors.rightMargin: Kirigami.Units.smallSpacing
        height: implicitHeight
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

    ColumnLayout {
        id: details

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: mainRow.bottom
        anchors.leftMargin: Kirigami.Units.smallSpacing
        anchors.rightMargin: Kirigami.Units.smallSpacing
        anchors.topMargin: Kirigami.Units.smallSpacing / 2
        visible: row.expanded
        height: implicitHeight
        spacing: Kirigami.Units.smallSpacing

        Text {
            Layout.fillWidth: true
            text: row.command
            color: Kirigami.Theme.textColor
            font.pixelSize: Kirigami.Theme.smallFont.pixelSize
            wrapMode: Text.WrapAnywhere
            textFormat: Text.PlainText
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Kirigami.Units.smallSpacing

            Text {
                text: i18n("PPID %1", row.ppid)
                color: Kirigami.Theme.disabledTextColor
                font: Kirigami.Theme.smallFont
            }

            Text {
                text: i18n("Memory %1%", Number(row.memoryPercent).toFixed(1))
                color: Kirigami.Theme.disabledTextColor
                font: Kirigami.Theme.smallFont
            }

            Item {
                Layout.fillWidth: true
            }

            ActionButton {
                visible: !row.confirmPending
                text: i18n("Terminate")
                onClicked: row.processActionRequested(false)
            }

            ActionButton {
                visible: !row.confirmPending
                text: i18n("Force kill")
                danger: true
                actionEnabled: row.pid > 1000
                onClicked: row.processActionRequested(true)
            }
        }

        RowLayout {
            Layout.fillWidth: true
            visible: row.confirmPending
            spacing: Kirigami.Units.smallSpacing

            Text {
                Layout.fillWidth: true
                text: row.confirmForce
                    ? i18n("Force kill %1 (PID %2) with SIGKILL?", row.name, row.pid)
                    : i18n("Terminate %1 (PID %2) with SIGTERM?", row.name, row.pid)
                color: Kirigami.Theme.textColor
                font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                wrapMode: Text.Wrap
            }

            ActionButton {
                text: i18n("Cancel")
                onClicked: row.actionCancelled()
            }

            ActionButton {
                text: row.confirmForce ? i18n("Force kill") : i18n("Terminate")
                danger: true
                onClicked: row.actionConfirmed()
            }
        }

        Text {
            Layout.fillWidth: true
            visible: row.actionError.length > 0
            text: row.actionError
            color: Kirigami.Theme.negativeTextColor
            font.pixelSize: Kirigami.Theme.smallFont.pixelSize
            wrapMode: Text.Wrap
        }
    }

    HoverHandler {
        id: hover
    }
}
