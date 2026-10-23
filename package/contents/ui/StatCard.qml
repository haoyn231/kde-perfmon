import QtQuick
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

Rectangle {
    id: card

    property string title
    property string valueText
    property string detail
    property string extra
    property real ratio: 0

    readonly property color surfaceColor: Qt.rgba(Kirigami.Theme.textColor.r, Kirigami.Theme.textColor.g, Kirigami.Theme.textColor.b, 0.06)
    readonly property color trackColor: Qt.rgba(Kirigami.Theme.textColor.r, Kirigami.Theme.textColor.g, Kirigami.Theme.textColor.b, 0.15)
    readonly property color barColor: card.ratio > 0.9
        ? Kirigami.Theme.negativeTextColor
        : (card.ratio > 0.75 ? Kirigami.Theme.neutralTextColor : Kirigami.Theme.highlightColor)

    implicitHeight: content.implicitHeight + Kirigami.Units.smallSpacing * 2
    radius: Kirigami.Units.smallSpacing
    color: card.surfaceColor

    ColumnLayout {
        id: content
        anchors.fill: parent
        anchors.margins: Kirigami.Units.smallSpacing
        spacing: Kirigami.Units.smallSpacing / 2

        RowLayout {
            Layout.fillWidth: true

            Text {
                text: card.title
                color: Kirigami.Theme.disabledTextColor
                font: Kirigami.Theme.smallFont
            }

            Item {
                Layout.fillWidth: true
            }

            Text {
                text: card.extra
                visible: card.extra.length > 0
                color: Kirigami.Theme.disabledTextColor
                font: Kirigami.Theme.smallFont
                elide: Text.ElideRight
            }
        }

        Text {
            text: card.valueText
            color: Kirigami.Theme.textColor
            font.pixelSize: Math.round(Kirigami.Theme.defaultFont.pixelSize * 1.55)
            font.weight: Font.DemiBold
            font.features: ({ "tnum": 1 })
        }

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 4
            radius: height / 2
            color: card.trackColor

            Rectangle {
                width: parent.width * Math.max(0, Math.min(1, card.ratio))
                height: parent.height
                radius: height / 2
                color: card.barColor
            }
        }

        Text {
            text: card.detail
            visible: card.detail.length > 0
            color: Kirigami.Theme.disabledTextColor
            font: Kirigami.Theme.smallFont
            elide: Text.ElideRight
            Layout.fillWidth: true
        }
    }
}
