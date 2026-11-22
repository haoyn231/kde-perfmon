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

        // 数字位宽不一样（"7.3%" 与 "100.0%" 差两个字符），按内容自适应会让
        // 面板上的图标随读数每秒变宽变窄。这里按最宽的可能取值预留固定位宽并
        // 右对齐：位数变化只在格子内左右挪动，图标整体宽度自始至终不变。
        // 读数范围 0–100、固定一位小数（Format.percent）：最长的是 "100.0%"；
        // 主题字体若没兑现 tnum，"8" 比 "1" 宽，量 "88.8%" 才够，两个取大。
        Text {
            id: valueLabel
            text: metric.valueText
            color: Kirigami.Theme.textColor
            font.features: ({ "tnum": 1 })
            Layout.preferredWidth: Math.max(longestSample.advanceWidth, widestSample.advanceWidth)
            horizontalAlignment: Text.AlignRight
        }

        TextMetrics {
            id: longestSample
            font: valueLabel.font
            text: "100.0%"
        }

        TextMetrics {
            id: widestSample
            font: valueLabel.font
            text: "88.8%"
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
