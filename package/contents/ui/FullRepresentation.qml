import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC2
import org.kde.kirigami as Kirigami

import "../lib/Hao/PerfMon"
import "Format.js" as Format

Item {
    id: full

    implicitWidth: Kirigami.Units.gridUnit * 27
    implicitHeight: Kirigami.Units.gridUnit * 35

    readonly property color subtleFill: Qt.rgba(Kirigami.Theme.textColor.r, Kirigami.Theme.textColor.g, Kirigami.Theme.textColor.b, 0.06)
    readonly property color subtleHover: Qt.rgba(Kirigami.Theme.textColor.r, Kirigami.Theme.textColor.g, Kirigami.Theme.textColor.b, 0.12)
    readonly property color dividerColor: Qt.rgba(Kirigami.Theme.textColor.r, Kirigami.Theme.textColor.g, Kirigami.Theme.textColor.b, 0.12)

    readonly property real cpuColumnWidth: Kirigami.Units.gridUnit * 3.6
    readonly property real memoryColumnWidth: Kirigami.Units.gridUnit * 5.2
    readonly property real pidColumnWidth: Kirigami.Units.gridUnit * 3.8

    property real savedScrollY: 0
    // Overlay scrollbars sit on top of the view; reserve their width so the
    // PID column stays readable instead of ending underneath the handle.
    readonly property real scrollbarWidth: verticalScrollBar.visible
        ? verticalScrollBar.width
        : 0

    Component.onCompleted: if (visible) PerfBackend.processListActive = true
    onVisibleChanged: if (visible) PerfBackend.processListActive = true

    // The sampler resorts and resets the model every second, which makes the
    // view jump back to the top. Restore the scroll position after each reset
    // so the popup stays usable while values keep updating.
    Connections {
        target: PerfBackend.processModel
        function onModelAboutToBeReset() {
            full.savedScrollY = processList.contentY;
        }
        function onModelReset() {
            Qt.callLater(function() {
                const maxY = Math.max(0, processList.contentHeight - processList.height);
                processList.contentY = Math.min(full.savedScrollY, maxY);
            });
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Kirigami.Units.largeSpacing
        spacing: Kirigami.Units.smallSpacing

        // System identity header: distro logo/name, hostname, avatar and user.
        RowLayout {
            Layout.fillWidth: true
            spacing: Kirigami.Units.smallSpacing

            Image {
                id: distroLogo
                visible: PerfBackend.distroLogoUrl !== ""
                source: PerfBackend.distroLogoUrl
                sourceSize.width: Kirigami.Units.iconSizes.medium * 2
                sourceSize.height: Kirigami.Units.iconSizes.medium * 2
                Layout.preferredWidth: Kirigami.Units.iconSizes.medium
                Layout.preferredHeight: Kirigami.Units.iconSizes.medium
                fillMode: Image.PreserveAspectFit
                smooth: true
                asynchronous: true
            }

            Kirigami.Icon {
                visible: PerfBackend.distroLogoUrl === ""
                source: PerfBackend.distroLogoIcon
                implicitWidth: Kirigami.Units.iconSizes.medium
                implicitHeight: Kirigami.Units.iconSizes.medium
                color: Kirigami.Theme.textColor
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                spacing: 0

                Text {
                    Layout.fillWidth: true
                    text: PerfBackend.distroName
                    color: Kirigami.Theme.textColor
                    font.weight: Font.DemiBold
                    elide: Text.ElideRight
                }

                Text {
                    Layout.fillWidth: true
                    text: PerfBackend.hostName
                    color: Kirigami.Theme.disabledTextColor
                    font: Kirigami.Theme.smallFont
                    elide: Text.ElideRight
                }
            }

            RowLayout {
                spacing: Kirigami.Units.smallSpacing

                Rectangle {
                    implicitWidth: Kirigami.Units.iconSizes.medium
                    implicitHeight: Kirigami.Units.iconSizes.medium
                    radius: width / 2
                    color: full.subtleFill
                    clip: true

                    Image {
                        id: userAvatar
                        objectName: "userAvatar"
                        anchors.fill: parent
                        source: PerfBackend.avatarUrl
                        sourceSize.width: Kirigami.Units.iconSizes.medium * 2
                        sourceSize.height: Kirigami.Units.iconSizes.medium * 2
                        fillMode: Image.PreserveAspectCrop
                        asynchronous: true
                        visible: status === Image.Ready
                    }

                    Kirigami.Icon {
                        anchors.centerIn: parent
                        source: "user-identity-symbolic"
                        implicitWidth: Kirigami.Units.iconSizes.smallMedium
                        implicitHeight: Kirigami.Units.iconSizes.smallMedium
                        color: Kirigami.Theme.textColor
                        visible: userAvatar.status !== Image.Ready
                    }
                }

                Text {
                    text: PerfBackend.userName
                    color: Kirigami.Theme.textColor
                    font: Kirigami.Theme.defaultFont
                    elide: Text.ElideRight
                    Layout.maximumWidth: Kirigami.Units.gridUnit * 8
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 1
            color: full.dividerColor
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Kirigami.Units.smallSpacing

            StatCard {
                Layout.fillWidth: true
                title: i18n("CPU")
                valueText: Format.percent(PerfBackend.cpuUsage)
                ratio: PerfBackend.cpuUsage / 100
                detail: i18n("%1 cores", PerfBackend.cpuCount)
            }

            StatCard {
                Layout.fillWidth: true
                title: i18n("Memory")
                valueText: Format.percent(PerfBackend.memoryUsage)
                ratio: PerfBackend.memoryUsage / 100
                detail: Format.formatBytes(PerfBackend.memoryUsedBytes) + " / " + Format.formatBytes(PerfBackend.memoryTotalBytes)
                extra: PerfBackend.swapTotalBytes > 0
                    ? i18n("Swap %1", Format.formatBytes(PerfBackend.swapUsedBytes))
                    : ""
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.topMargin: Kirigami.Units.smallSpacing
            spacing: Kirigami.Units.smallSpacing

            Text {
                text: i18n("Processes")
                color: Kirigami.Theme.textColor
                font.weight: Font.DemiBold
            }

            Text {
                text: i18n("%1 total", PerfBackend.processCount)
                color: Kirigami.Theme.disabledTextColor
                font: Kirigami.Theme.smallFont
            }

            Item {
                Layout.fillWidth: true
            }

            SortChip {
                text: i18n("CPU")
                active: PerfBackend.sortKey === "cpu"
                onClicked: PerfBackend.toggleSort("cpu")
            }

            SortChip {
                text: i18n("Memory")
                active: PerfBackend.sortKey === "memory"
                onClicked: PerfBackend.toggleSort("memory")
            }

            Rectangle {
                id: directionButton

                implicitWidth: Kirigami.Units.gridUnit * 1.55
                implicitHeight: Kirigami.Units.gridUnit * 1.55
                radius: height / 2
                color: directionHover.hovered ? full.subtleHover : "transparent"

                Kirigami.Icon {
                    anchors.centerIn: parent
                    source: PerfBackend.sortAscending ? "arrow-up-symbolic" : "arrow-down-symbolic"
                    implicitWidth: Kirigami.Units.iconSizes.small
                    implicitHeight: Kirigami.Units.iconSizes.small
                    color: Kirigami.Theme.textColor
                }

                HoverHandler {
                    id: directionHover
                    cursorShape: Qt.PointingHandCursor
                }

                TapHandler {
                    acceptedButtons: Qt.LeftButton
                    onTapped: PerfBackend.toggleSort(PerfBackend.sortKey)
                }

                Accessible.role: Accessible.Button
                Accessible.name: PerfBackend.sortAscending ? i18n("Ascending") : i18n("Descending")
                Accessible.onPressAction: PerfBackend.toggleSort(PerfBackend.sortKey)

                QQC2.ToolTip.visible: directionHover.hovered
                QQC2.ToolTip.delay: 600
                QQC2.ToolTip.text: PerfBackend.sortAscending ? i18n("Ascending") : i18n("Descending")
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: Kirigami.Units.smallSpacing
            Layout.rightMargin: Kirigami.Units.smallSpacing + full.scrollbarWidth
            spacing: Kirigami.Units.smallSpacing

            Text {
                Layout.fillWidth: true
                text: i18n("Process")
                color: Kirigami.Theme.disabledTextColor
                font: Kirigami.Theme.smallFont
                elide: Text.ElideRight
            }

            Text {
                Layout.preferredWidth: full.cpuColumnWidth
                text: i18n("CPU")
                color: Kirigami.Theme.disabledTextColor
                font: Kirigami.Theme.smallFont
                horizontalAlignment: Text.AlignRight
            }

            Text {
                Layout.preferredWidth: full.memoryColumnWidth
                text: i18n("Memory")
                color: Kirigami.Theme.disabledTextColor
                font: Kirigami.Theme.smallFont
                horizontalAlignment: Text.AlignRight
            }

            Text {
                Layout.preferredWidth: full.pidColumnWidth
                text: i18n("PID")
                color: Kirigami.Theme.disabledTextColor
                font: Kirigami.Theme.smallFont
                horizontalAlignment: Text.AlignRight
            }
        }

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 1
            color: full.dividerColor
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            ListView {
                id: processList
                objectName: "processList"

                anchors.fill: parent
                clip: true
                model: PerfBackend.processModel
                reuseItems: true
                spacing: 0

                QQC2.ScrollBar.vertical: QQC2.ScrollBar {
                    id: verticalScrollBar
                    objectName: "processScrollBar"
                    policy: QQC2.ScrollBar.AsNeeded
                }

                delegate: ProcessRow {
                    width: processList.width - full.scrollbarWidth
                    cpuColumnWidth: full.cpuColumnWidth
                    memoryColumnWidth: full.memoryColumnWidth
                    pidColumnWidth: full.pidColumnWidth
                }
            }

            Text {
                anchors.centerIn: parent
                visible: PerfBackend.processCount === 0
                text: i18n("Loading processes…")
                color: Kirigami.Theme.disabledTextColor
                font: Kirigami.Theme.defaultFont
            }
        }
    }
}
