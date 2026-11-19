// 每核负载：一条横带，每根柱子对应 /proc/stat 的一个 cpuN（线程）。
//
// 数据来自 PerfBackend.coreUsage：后端按线程读取 cpuN 行做差分，这里只负责画。
// 柱子数量跟着线程数走（24 线程就是 24 根），所以 P/E 核混排、SMT 开关都不用特判。

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC2
import org.kde.kirigami as Kirigami

Item {
    id: section

    property var usage: []                 // 每个线程的占用百分比，下标 == cpuN
    readonly property int count: usage ? usage.length : 0
    readonly property real peak: {
        let top = 0;
        for (let i = 0; i < count; ++i) {
            const value = Number(usage[i]) || 0;
            if (value > top) {
                top = value;
            }
        }
        return top;
    }

    // 16 根柱子以下每根都标号；再密就隔一根标一次，免得数字糊成一片。
    readonly property int labelStep: Math.max(1, Math.ceil(count / 12))
    readonly property real barGap: Math.max(1, Math.round(Kirigami.Units.smallSpacing / 2))
    readonly property real graphHeight: Math.round(Kirigami.Units.gridUnit * 2.4)
    // 核心很少时别把柱子拉成肥条：限宽后整条横带居中。
    readonly property real barWidth: count > 0
        ? Math.min((width - barGap * (count - 1)) / count, Kirigami.Units.gridUnit * 2.5)
        : 0
    readonly property real stripWidth: count > 0 ? barWidth * count + barGap * (count - 1) : 0
    readonly property bool showLabels: count > 0 && barWidth >= 12
    readonly property real labelHeight: showLabels ? Math.round(Kirigami.Units.gridUnit * 0.95) : 0

    readonly property color textColor: Kirigami.Theme.textColor
    readonly property color dimColor: Kirigami.Theme.disabledTextColor
    readonly property color trackColor: Qt.rgba(textColor.r, textColor.g, textColor.b, 0.06)

    Accessible.name: i18n("Per-core load, %1 threads, peak %2 percent", count, Math.round(peak))

    implicitHeight: header.implicitHeight + Math.round(Kirigami.Units.smallSpacing / 2) + graphHeight + labelHeight
    implicitWidth: Kirigami.Units.gridUnit * 18
    visible: count > 0

    // 和进程列表同一套阈值：90% 以上算过载，70% 以上提醒一眼。
    function colorFor(percent) {
        if (percent >= 90) {
            return Kirigami.Theme.negativeTextColor;
        }
        if (percent >= 70) {
            return Kirigami.Theme.neutralTextColor;
        }
        return Kirigami.Theme.highlightColor;
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: Math.round(Kirigami.Units.smallSpacing / 2)

        RowLayout {
            id: header
            Layout.fillWidth: true
            spacing: Kirigami.Units.smallSpacing

            Text {
                text: i18n("Per-core load")
                color: section.dimColor
                font: Kirigami.Theme.smallFont
            }

            Item {
                Layout.fillWidth: true
            }

            Text {
                text: i18n("%1 threads · peak %2%", section.count, Math.round(section.peak))
                color: section.dimColor
                font: Kirigami.Theme.smallFont
                elide: Text.ElideLeft
            }
        }

        Item {
            id: graph
            Layout.fillWidth: true
            Layout.preferredHeight: section.graphHeight + section.labelHeight

            Row {
                anchors.top: parent.top
                anchors.horizontalCenter: parent.horizontalCenter
                width: section.stripWidth
                height: parent.height
                spacing: section.barGap

                Repeater {
                    model: section.count

                    delegate: Item {
                        id: slot

                        readonly property real percent: Number(section.usage[index]) || 0

                        width: Math.max(1, section.barWidth)
                        height: graph.height

                        // 30% 以下的柱子在细窄时几乎看不见，最矮也留 1px。
                        Rectangle {
                            id: track
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.top: parent.top
                            height: section.graphHeight
                            radius: Math.min(2, width / 2.5)
                            color: section.trackColor
                        }

                        Rectangle {
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.bottom: track.bottom
                            height: slot.percent > 0
                                ? Math.max(1, track.height * Math.min(100, slot.percent) / 100)
                                : 0
                            radius: Math.min(2, width / 2.5)
                            color: section.colorFor(slot.percent)

                            Behavior on height {
                                NumberAnimation {
                                    duration: Kirigami.Units.shortDuration
                                    easing.type: Easing.OutCubic
                                }
                            }
                            Behavior on color {
                                ColorAnimation { duration: Kirigami.Units.shortDuration }
                            }
                        }

                        Text {
                            anchors.top: track.bottom
                            anchors.topMargin: 1
                            anchors.horizontalCenter: parent.horizontalCenter
                            visible: section.showLabels && index % section.labelStep === 0
                            text: index
                            color: section.dimColor
                            font.pixelSize: Math.max(7, Math.round(Math.min(Kirigami.Units.gridUnit * 0.5, slot.width * 0.72)))
                            font.features: ({ "tnum": 1 })
                        }

                        HoverHandler {
                            id: barHover
                        }

                        QQC2.ToolTip.visible: barHover.hovered
                        QQC2.ToolTip.delay: 250
                        QQC2.ToolTip.text: i18n("Thread %1 · %2%", index, slot.percent.toFixed(0))
                    }
                }
            }
        }
    }
}
