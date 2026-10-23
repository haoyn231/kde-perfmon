import QtQuick
import QtQuick.Layouts
import org.kde.plasma.plasmoid
import org.kde.plasma.core as PlasmaCore
import org.kde.kirigami as Kirigami

import "../lib/Hao/PerfMon"
import "Format.js" as Format

Item {
    id: compact

    signal activated(string sortKey)

    readonly property bool vertical: Plasmoid.formFactor === PlasmaCore.Types.Vertical
    readonly property int iconSize: Kirigami.Units.iconSizes.small

    implicitWidth: vertical
        ? iconSize + Kirigami.Units.smallSpacing * 2
        : metrics.implicitWidth + Kirigami.Units.smallSpacing * 2
    implicitHeight: vertical
        ? metrics.implicitHeight + Kirigami.Units.smallSpacing
        : iconSize + Kirigami.Units.smallSpacing

    // The panel lays applets out through Layout attached properties; without
    // these it falls back to a small icon-sized slot.
    Layout.preferredWidth: implicitWidth
    Layout.minimumWidth: implicitWidth
    Layout.maximumWidth: implicitWidth
    Layout.preferredHeight: implicitHeight
    Layout.minimumHeight: implicitHeight
    Layout.maximumHeight: implicitHeight

    Accessible.name: i18n("CPU %1, memory %2", Format.percent(PerfBackend.cpuUsage), Format.percent(PerfBackend.memoryUsage))
    Accessible.role: Accessible.Button

    GridLayout {
        id: metrics
        anchors.centerIn: parent
        columns: compact.vertical ? 1 : 2
        columnSpacing: Kirigami.Units.smallSpacing
        rowSpacing: 0

        Metric {
            iconSource: "speedometer-symbolic"
            valueText: Format.percent(PerfBackend.cpuUsage)
            onActivated: compact.activated("cpu")
        }

        Metric {
            iconSource: "dialog-memory-symbolic"
            valueText: Format.percent(PerfBackend.memoryUsage)
            onActivated: compact.activated("memory")
        }
    }

    component Metric: RowLayout {
        id: metric

        property string iconSource
        property string valueText
        signal activated

        spacing: Kirigami.Units.smallSpacing / 2

        Kirigami.Icon {
            source: metric.iconSource
            implicitWidth: compact.iconSize
            implicitHeight: compact.iconSize
            color: Kirigami.Theme.textColor
        }

        Text {
            text: metric.valueText
            color: Kirigami.Theme.textColor
            font.features: ({ "tnum": 1 })
        }

        HoverHandler {
            cursorShape: Qt.PointingHandCursor
        }

        TapHandler {
            acceptedButtons: Qt.LeftButton
            onTapped: metric.activated()
        }
    }
}
