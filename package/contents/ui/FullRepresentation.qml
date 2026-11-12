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

    // Opaque tint of the popup background towards the text color: the floating
    // button must stay readable while rows scroll behind it.
    function tintedBackground(amount) {
        return Qt.rgba(
            Kirigami.Theme.backgroundColor.r * (1 - amount) + Kirigami.Theme.textColor.r * amount,
            Kirigami.Theme.backgroundColor.g * (1 - amount) + Kirigami.Theme.textColor.g * amount,
            Kirigami.Theme.backgroundColor.b * (1 - amount) + Kirigami.Theme.textColor.b * amount,
            1);
    }

    // Overlay scrollbars sit on top of the view; reserve their width so the
    // PID column stays readable instead of ending underneath the handle.
    readonly property real scrollbarWidth: verticalScrollBar.visible
        ? verticalScrollBar.width
        : 0

    // Distance covered by one mouse-wheel notch; a notch is 120 units.
    readonly property real wheelStep: Kirigami.Units.gridUnit * 2.5
    // Anything above this counts as "scrolled away from the top".
    readonly property real topSnap: 1.0

    property string expandedPid: ""
    property int pendingKillPid: 0
    property bool pendingKillForce: false
    property string actionError: ""

    // The table only follows the sampler while it is pinned to the top. As soon
    // as the reader scrolls away the rows freeze, because a list that re-sorts
    // itself every second cannot be read or clicked reliably.
    function syncLiveState() {
        PerfBackend.processListPaused = processList.contentY > topSnap;
    }

    function scrollToTop() {
        if (processList.contentY <= topSnap) {
            processList.contentY = 0;
            syncLiveState();
            return;
        }
        scrollToTopAnimation.restart();
    }

    // Sorting rebuilds the model, so show the top of the new order and resume
    // live updates instead of leaving the view at an arbitrary offset.
    function resetViewToTop() {
        scrollToTopAnimation.stop();
        processList.contentY = 0;
        syncLiveState();
    }

    function toggleExpanded(pid) {
        const id = String(pid);
        expandedPid = expandedPid === id ? "" : id;
        pendingKillPid = 0;
        actionError = "";
    }

    function requestProcessAction(pid, force) {
        pendingKillPid = pid;
        pendingKillForce = force;
        actionError = "";
    }

    function cancelProcessAction() {
        pendingKillPid = 0;
    }

    function confirmProcessAction() {
        const pid = pendingKillPid;
        const force = pendingKillForce;
        pendingKillPid = 0;
        if (pid <= 0) {
            return;
        }
        actionError = force ? PerfBackend.killProcess(pid) : PerfBackend.terminateProcess(pid);
        if (actionError.length === 0) {
            // Drop the row immediately; the next scan would not happen at all
            // while the table is frozen.
            PerfBackend.removeProcess(pid);
            PerfBackend.refresh();
        }
    }

    Component.onCompleted: {
        if (visible) {
            PerfBackend.processListActive = true;
        }
        syncLiveState();
    }
    onVisibleChanged: {
        if (visible) {
            PerfBackend.processListActive = true;
            syncLiveState();
        } else {
            expandedPid = "";
            pendingKillPid = 0;
            actionError = "";
            resetViewToTop();
        }
    }

    // Drives the floating "back to top" button; stops as soon as the reader
    // grabs the list, the wheel or the scrollbar again.
    NumberAnimation {
        id: scrollToTopAnimation
        target: processList
        property: "contentY"
        to: 0
        duration: Kirigami.Units.longDuration
        easing.type: Easing.OutCubic
    }

    Connections {
        target: processList
        function onContentYChanged() {
            full.syncLiveState();
        }
        function onMovementStarted() {
            scrollToTopAnimation.stop();
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

            RowLayout {
                id: pausedBadge
                objectName: "pausedBadge"

                // Only shown while the rows are frozen, so the still table is
                // never mistaken for a system that stopped changing.
                visible: PerfBackend.processListPaused
                spacing: Kirigami.Units.smallSpacing / 1.5

                // A plain dot instead of a theme icon: symbolic icon colors are
                // not reliable across icon themes, and the state has to read as
                // one unit next to its label.
                Rectangle {
                    implicitWidth: Kirigami.Units.smallSpacing * 1.2
                    implicitHeight: implicitWidth
                    radius: width / 2
                    color: Kirigami.Theme.neutralTextColor
                }

                Text {
                    text: i18n("Paused")
                    color: Kirigami.Theme.neutralTextColor
                    font: Kirigami.Theme.smallFont
                }

                HoverHandler {
                    id: pausedHover
                }

                QQC2.ToolTip.visible: pausedHover.hovered
                QQC2.ToolTip.delay: 400
                QQC2.ToolTip.text: i18n("Rows stop updating once you scroll away. Scroll back to the top to go live again.")
            }

            Item {
                Layout.fillWidth: true
            }

            SortChip {
                text: i18n("CPU")
                active: PerfBackend.sortKey === "cpu"
                onClicked: {
                    full.resetViewToTop();
                    PerfBackend.toggleSort("cpu");
                }
            }

            SortChip {
                text: i18n("Memory")
                active: PerfBackend.sortKey === "memory"
                onClicked: {
                    full.resetViewToTop();
                    PerfBackend.toggleSort("memory");
                }
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
                    onTapped: {
                        full.resetViewToTop();
                        PerfBackend.toggleSort(PerfBackend.sortKey);
                    }
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

                    onPressedChanged: {
                        if (pressed) {
                            scrollToTopAnimation.stop();
                        }
                    }
                }

                // Flickable's own wheel handling stops accepting events after the
                // first notch, which made the wheel feel dead. Scrolling is
                // driven explicitly here for both the wheel and the touchpad,
                // including while the pointer sits on the scrollbar handle.
                WheelHandler {
                    id: listWheel
                    acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
                    onWheel: (wheel) => {
                        scrollToTopAnimation.stop();
                        const maxY = Math.max(0, processList.contentHeight - processList.height);
                        const delta = wheel.pixelDelta.y !== 0
                            ? wheel.pixelDelta.y
                            : (wheel.angleDelta.y / 120) * full.wheelStep;
                        processList.contentY = Math.max(0, Math.min(maxY, processList.contentY - delta));
                    }
                }

                delegate: ProcessRow {
                    width: processList.width - full.scrollbarWidth
                    cpuColumnWidth: full.cpuColumnWidth
                    memoryColumnWidth: full.memoryColumnWidth
                    pidColumnWidth: full.pidColumnWidth

                    expanded: full.expandedPid === String(pid)
                    confirmPending: full.pendingKillPid === pid
                    confirmForce: full.pendingKillForce
                    actionError: full.expandedPid === String(pid) ? full.actionError : ""

                    onToggleRequested: full.toggleExpanded(pid)
                    onProcessActionRequested: (force) => full.requestProcessAction(pid, force)
                    onActionConfirmed: full.confirmProcessAction()
                    onActionCancelled: full.cancelProcessAction()
                }
            }

            // Floating shortcut back to the live top of the list; it only shows
            // up while the reader is scrolled away.
            Rectangle {
                id: backToTopButton
                objectName: "backToTopButton"

                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.rightMargin: Kirigami.Units.smallSpacing + full.scrollbarWidth
                anchors.bottomMargin: Kirigami.Units.smallSpacing
                width: Kirigami.Units.gridUnit * 1.9
                height: width
                radius: width / 2
                visible: processList.contentY > full.topSnap
                color: full.tintedBackground(backToTopHover.hovered ? 0.22 : 0.14)
                border.width: 1
                border.color: full.dividerColor

                Kirigami.Icon {
                    anchors.centerIn: parent
                    source: "go-up-symbolic"
                    implicitWidth: Kirigami.Units.iconSizes.smallMedium
                    implicitHeight: Kirigami.Units.iconSizes.smallMedium
                    color: Kirigami.Theme.textColor
                }

                HoverHandler {
                    id: backToTopHover
                    cursorShape: Qt.PointingHandCursor
                }

                TapHandler {
                    acceptedButtons: Qt.LeftButton
                    onTapped: full.scrollToTop()
                }

                Accessible.role: Accessible.Button
                Accessible.name: i18n("Back to top")
                Accessible.description: i18n("Resume live updates")
                Accessible.onPressAction: full.scrollToTop()

                QQC2.ToolTip.visible: backToTopHover.hovered
                QQC2.ToolTip.delay: 600
                QQC2.ToolTip.text: i18n("Back to top")
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
