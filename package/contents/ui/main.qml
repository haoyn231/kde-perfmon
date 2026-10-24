import QtQuick
import org.kde.plasma.plasmoid
import "../lib/Hao/PerfMon"
import "Format.js" as Format

PlasmoidItem {
    id: root

    Plasmoid.title: i18n("Performance Monitor")
    toolTipMainText: i18n("Performance Monitor")
    toolTipSubText: i18n("CPU %1 · Memory %2", Format.percent(PerfBackend.cpuUsage), Format.percent(PerfBackend.memoryUsage))

    preferredRepresentation: compactRepresentation

    compactRepresentation: CompactRepresentation {
        onActivated: (sortKey) => {
            if (root.expanded) {
                root.expanded = false;
                return;
            }
            PerfBackend.setSortKey(sortKey);
            root.expanded = true;
        }
    }

    fullRepresentation: FullRepresentation {}

    // Scanning /proc/<pid> every second is only worth it while the popup is
    // open; the panel itself needs the two totals only.
    onExpandedChanged: function() { PerfBackend.processListActive = root.expanded; }
    Component.onCompleted: PerfBackend.processListActive = expanded
}
