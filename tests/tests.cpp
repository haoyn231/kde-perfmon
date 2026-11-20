#include "perfbackend.h"
#include "procdata.h"

#include <QCoreApplication>
#include <QFileInfo>
#include <QProcess>
#include <QUrl>
#include <QtTest>

class PerfTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void parseCpuTimes();
    void rejectForeignCpuLine();
    void parseCoreCpuLine();
    void cpuBusyPercentMath();
    void countPhysicalCoresFromCpuinfo();
    void raplPowerMath();
    void parseProcStatWithParens();
    void cmdlineParsing();
    void processCpuMath();
    void sortByCpuAndMemory();
    void sortIgnoresCpuNoise();
    void processModelIncrementalSync();
    void liveSampling();
    void processListPause();
    void removeProcess();
    void processSignals();
};

void PerfTest::parseCpuTimes()
{
    CpuTimes times;
    QVERIFY(parseCpuTimesLine("cpu  123 45 67 890 12 34 56 78 0 0", &times));
    QCOMPARE(times.user, quint64(123));
    QCOMPARE(times.nice, quint64(45));
    QCOMPARE(times.system, quint64(67));
    QCOMPARE(times.idle, quint64(890));
    QCOMPARE(times.iowait, quint64(12));
    QCOMPARE(times.total(), quint64(123 + 45 + 67 + 890 + 12 + 34 + 56 + 78));
    QCOMPARE(times.idleAll(), quint64(902));
}

void PerfTest::rejectForeignCpuLine()
{
    CpuTimes times;
    QVERIFY(!parseCpuTimesLine("cpu0 1 2 3 4 5 6 7 8", &times));
    QVERIFY(!parseCpuTimesLine("intr 1 2 3", &times));
}

void PerfTest::parseCoreCpuLine()
{
    CpuTimes times;
    int index = -1;
    QVERIFY(parseCoreCpuTimesLine("cpu7 10 20 30 40 50 60 70 80 0 0\n", &index, &times));
    QCOMPARE(index, 7);
    QCOMPARE(times.user, quint64(10));
    QCOMPARE(times.idle, quint64(40));
    QCOMPARE(times.total(), quint64(360));

    // Only digit-labelled lines are core lines; the aggregate and everything
    // else stays with parseCpuTimesLine().
    QVERIFY(!parseCoreCpuTimesLine("cpu  123 45 67 890 12 34 56 78", &index, &times));
    QVERIFY(!parseCoreCpuTimesLine("cpu0x 1 2 3 4 5 6 7 8", &index, &times));
    QVERIFY(!parseCoreCpuTimesLine("intr 1 2 3 4 5 6 7 8", &index, &times));
    QVERIFY(!parseCoreCpuTimesLine("cpu3", &index, &times));

    // An index pointer is optional.
    QVERIFY(parseCoreCpuTimesLine("cpu0 1 2 3 4 5 6 7 8", nullptr, &times));
}

void PerfTest::cpuBusyPercentMath()
{
    CpuTimes previous;
    previous.user = 100;
    previous.idle = 100;

    CpuTimes half;
    half.user = 150;
    half.idle = 150;
    QVERIFY(qAbs(cpuBusyPercent(previous, half) - 50.0) < 0.0001);

    CpuTimes idleOnly;
    idleOnly.user = 100;
    idleOnly.idle = 200;
    QCOMPARE(cpuBusyPercent(previous, idleOnly), 0.0);

    CpuTimes full;
    full.user = 300;
    full.idle = 100;
    QCOMPARE(cpuBusyPercent(previous, full), 100.0);

    // A sample window where the counters did not move at all yields nothing.
    QCOMPARE(cpuBusyPercent(previous, previous), 0.0);
}

void PerfTest::countPhysicalCoresFromCpuinfo()
{
    const QByteArray twoSockets =
        "processor\t: 0\n"
        "physical id\t: 0\n"
        "siblings\t: 4\n"
        "core id\t\t: 0\n"
        "cpu cores\t: 2\n"
        "\n"
        "processor\t: 1\n"
        "physical id\t: 0\n"
        "core id\t\t: 1\n"
        "\n"
        "processor\t: 2\n"
        "physical id\t: 1\n"
        "core id\t\t: 0\n"
        "\n"
        "processor\t: 3\n"
        "physical id\t: 1\n"
        "core id\t\t: 1\n"
        "\n";
    QCOMPARE(countPhysicalCores(twoSockets), 4);

    // Repeated (physical, core) pairs are SMT siblings, not extra cores.
    QByteArray smt = twoSockets;
    smt += "processor\t: 4\nphysical id\t: 0\ncore id\t\t: 0\n\n";
    QCOMPARE(countPhysicalCores(smt), 4);

    // No topology fields -> 0, so the caller falls back to the thread count.
    QCOMPARE(countPhysicalCores("processor\t: 0\nmodel name\t: whatever\n\n"), 0);
    QCOMPARE(countPhysicalCores(QByteArray()), 0);
}

void PerfTest::raplPowerMath()
{
    // 1 J of energy over 1 s, expressed in RAPL's microjoules and nanoseconds.
    QVERIFY(qAbs(raplPowerWatts(1000000, 2000000, 262143328850ULL, 1000000000LL) - 1.0) < 0.0001);
    // 30 J over 1 s is 30 W.
    QVERIFY(qAbs(raplPowerWatts(0, 30000000, 0, 1000000000LL) - 30.0) < 0.0001);

    // Counter wrapped around: (max - previous) + current.
    QVERIFY(qAbs(raplPowerWatts(99900000, 100000, 100000000, 1000000000LL) - 0.2) < 0.0001);

    // Went backwards with no known range -> treated as a reset, not a huge
    // negative power draw.
    QCOMPARE(raplPowerWatts(5000, 1000, 0, 1000000000LL), 0.0);
    // No time elapsed -> nothing to divide by.
    QCOMPARE(raplPowerWatts(0, 1000000, 0, 0), 0.0);
}

void PerfTest::parseProcStatWithParens()
{
    const QByteArray line =
        "1234 (my (odd) proc) S 1 0 0 0 0 0 0 0 0 0 100 200 0 0 20 0 1 0 0 0 4096 12345";
    ProcStat stat;
    QVERIFY(parseProcStat(line, &stat, 4096));
    QCOMPARE(stat.pid, 1234);
    QCOMPARE(stat.ppid, 1);
    QCOMPARE(stat.name, QStringLiteral("my (odd) proc"));
    QCOMPARE(stat.cpuTicks, quint64(300));
    QCOMPARE(stat.rssBytes, quint64(4096) * 4096);
}

void PerfTest::cmdlineParsing()
{
    const QByteArray cmdline("/usr/bin/foo\0--bar\0baz\0", 24);
    QCOMPARE(parseCmdline(cmdline), QStringLiteral("/usr/bin/foo --bar baz"));
    QCOMPARE(parseCmdline(QByteArray()).isEmpty(), true);
}

void PerfTest::processCpuMath()
{
    // 100 ticks in one second at 100 Hz is one full core on a 24 core machine.
    QVERIFY(qAbs(processCpuPercent(100, 1000, 100, 24) - 100.0 / 24.0) < 0.0001);
    QCOMPARE(processCpuPercent(0, 1000, 100, 24), 0.0);
    QCOMPARE(processCpuPercent(100, 0, 100, 24), 0.0);
    QCOMPARE(processCpuPercent(1000000, 1000, 100, 1), 100.0);
}

void PerfTest::sortByCpuAndMemory()
{
    auto make = [](int pid, double cpu, quint64 memory) {
        ProcessInfo info;
        info.pid = pid;
        info.name = QString::number(pid);
        info.command = info.name;
        info.cpu = cpu;
        info.memoryBytes = memory;
        return info;
    };

    QVector<ProcessInfo> processes{make(1, 2.0, 900), make(2, 5.0, 100), make(3, 5.0, 500)};

    sortProcesses(processes, QStringLiteral("cpu"), false);
    QCOMPARE(processes.at(0).pid, 2); // 5.0, tie broken by pid
    QCOMPARE(processes.at(1).pid, 3);
    QCOMPARE(processes.at(2).pid, 1);

    sortProcesses(processes, QStringLiteral("cpu"), true);
    QCOMPARE(processes.at(0).pid, 1);
    QCOMPARE(processes.at(1).pid, 2);
    QCOMPARE(processes.at(2).pid, 3);

    sortProcesses(processes, QStringLiteral("memory"), false);
    QCOMPARE(processes.at(0).pid, 1);
    QCOMPARE(processes.at(1).pid, 3);
    QCOMPARE(processes.at(2).pid, 2);

    sortProcesses(processes, QStringLiteral("memory"), true);
    QCOMPARE(processes.at(0).pid, 2);
}

// Sub-0.1% CPU noise must not count as a real difference: otherwise the idle
// tail of the table gets a different order on every sample and the row-move
// animation turns into permanent churn.
void PerfTest::sortIgnoresCpuNoise()
{
    auto make = [](int pid, double cpu) {
        ProcessInfo info;
        info.pid = pid;
        info.name = QString::number(pid);
        info.command = info.name;
        info.cpu = cpu;
        return info;
    };

    QVector<ProcessInfo> processes{make(9, 0.004), make(3, 0.001), make(7, 0.008)};
    sortProcesses(processes, QStringLiteral("cpu"), false);
    // All three show "0.0%", so they keep PID order instead of chasing noise.
    QCOMPARE(processes.at(0).pid, 3);
    QCOMPARE(processes.at(1).pid, 7);
    QCOMPARE(processes.at(2).pid, 9);

    processes = {make(9, 0.004), make(3, 0.24), make(7, 0.008)};
    sortProcesses(processes, QStringLiteral("cpu"), false);
    // A visible difference (0.2% vs 0.0%) still sorts.
    QCOMPARE(processes.at(0).pid, 3);
    QCOMPARE(processes.at(1).pid, 7);
    QCOMPARE(processes.at(2).pid, 9);
}

// The view animates row movements, which only works while the model keeps its
// rows alive: a re-sort has to move them, a value change has to patch them in
// place, and only genuinely new / gone PIDs may insert or remove.
void PerfTest::processModelIncrementalSync()
{
    auto make = [](int pid, double cpu) {
        ProcessInfo info;
        info.pid = pid;
        info.name = QStringLiteral("proc-%1").arg(pid);
        info.command = info.name;
        info.cpu = cpu;
        info.memoryBytes = quint64(pid) * 1024;
        return info;
    };

    ProcessModel model;
    QSignalSpy resetSpy(&model, &QAbstractItemModel::modelReset);
    QSignalSpy moveSpy(&model, &QAbstractItemModel::rowsMoved);
    QSignalSpy insertSpy(&model, &QAbstractItemModel::rowsInserted);
    QSignalSpy removeSpy(&model, &QAbstractItemModel::rowsRemoved);
    QSignalSpy changeSpy(&model, &QAbstractItemModel::dataChanged);

    model.syncProcesses({make(1, 1.0), make(2, 2.0), make(3, 3.0)});
    QCOMPARE(model.rowCount(), 3);
    QCOMPARE(insertSpy.count(), 3);
    QCOMPARE(changeSpy.count(), 0);

    // A re-sort moves the existing rows instead of rebuilding the model.
    insertSpy.clear();
    moveSpy.clear();
    model.syncProcesses({make(3, 3.0), make(1, 1.0), make(2, 2.0)});
    QCOMPARE(resetSpy.count(), 0);
    QVERIFY2(moveSpy.count() >= 1, "a re-sort must move rows, not re-insert them");
    QCOMPARE(insertSpy.count(), 0);
    QCOMPARE(model.index(0, 0).data(ProcessModel::PidRole).toInt(), 3);
    QCOMPARE(model.index(1, 0).data(ProcessModel::PidRole).toInt(), 1);
    QCOMPARE(model.index(2, 0).data(ProcessModel::PidRole).toInt(), 2);

    // A changed value patches one role in place and reports only that role.
    moveSpy.clear();
    changeSpy.clear();
    model.syncProcesses({make(3, 9.5), make(1, 1.0), make(2, 2.0)});
    QCOMPARE(moveSpy.count(), 0);
    QCOMPARE(changeSpy.count(), 1);
    QCOMPARE(changeSpy.at(0).at(2).value<QList<int>>(), QList<int>{ProcessModel::CpuRole});
    QCOMPARE(model.index(0, 0).data(ProcessModel::CpuRole).toDouble(), 9.5);

    // Nothing changed at all: not a single signal.
    changeSpy.clear();
    model.syncProcesses({make(3, 9.5), make(1, 1.0), make(2, 2.0)});
    QCOMPARE(changeSpy.count(), 0);

    // PIDs that exited disappear, unknown PIDs are inserted.
    moveSpy.clear();
    removeSpy.clear();
    insertSpy.clear();
    model.syncProcesses({make(4, 4.0), make(3, 9.5)});
    QCOMPARE(resetSpy.count(), 0);
    QCOMPARE(model.rowCount(), 2);
    QCOMPARE(model.index(0, 0).data(ProcessModel::PidRole).toInt(), 4);
    QVERIFY(insertSpy.count() >= 1);
    QVERIFY(removeSpy.count() >= 1);

    // An empty scan empties the model without a reset.
    model.syncProcesses({});
    QCOMPARE(model.rowCount(), 0);
    QCOMPARE(resetSpy.count(), 0);
}

void PerfTest::liveSampling()
{
    PerfBackend backend;
    QVERIFY(!backend.distroName().isEmpty());
    QVERIFY(!backend.distroLogoIcon().isEmpty());
    QVERIFY(!backend.hostName().isEmpty());
    QVERIFY(!backend.userName().isEmpty());
    if (!backend.distroLogoUrl().isEmpty()) {
        QVERIFY2(QFileInfo(QUrl(backend.distroLogoUrl()).toLocalFile()).isFile(), "distro logo URL must point at a file");
    }
    if (!backend.avatarUrl().isEmpty()) {
        QVERIFY2(QFileInfo(QUrl(backend.avatarUrl()).toLocalFile()).isFile(), "avatar URL must point at a file");
    }
    QVERIFY(backend.cpuCount() >= 1);
    // Best effort: 0 means /proc/cpuinfo carried no topology at all.
    QVERIFY(backend.physicalCpuCount() >= 0);
    QVERIFY(backend.memoryTotalBytes() > 0);
    QVERIFY(backend.memoryUsedBytes() <= backend.memoryTotalBytes());

    // The per-thread strip needs one delta window before it reports anything.
    QTRY_VERIFY_WITH_TIMEOUT(!backend.coreUsage().isEmpty(), 4000);
    const QVariantList threadUsage = backend.coreUsage();
    QVERIFY(threadUsage.size() >= 1);
    for (const QVariant &sample : threadUsage) {
        const double percent = sample.toDouble();
        QVERIFY2(percent >= 0.0 && percent <= 100.0, "per-thread usage out of range");
    }
    // Package power is host dependent: only the range is guaranteed. When the
    // kernel does expose a counter, the smoothed value must be positive.
    QVERIFY(backend.cpuPowerWatts() >= 0.0);
    if (backend.cpuPowerAvailable()) {
        QVERIFY(backend.cpuPowerWatts() > 0.0);
    }

    backend.setProcessListActive(true);
    QTRY_VERIFY_WITH_TIMEOUT(backend.processCount() > 0, 3000);

    // Give the sampler one delta window so CPU values are computed.
    QTest::qWait(1200);
    backend.refresh();

    QAbstractListModel *model = backend.processModel();
    QVERIFY(model != nullptr);
    QVERIFY(model->rowCount() > 0);

    bool foundSelf = false;
    for (int row = 0; row < model->rowCount(); ++row) {
        const QModelIndex index = model->index(row, 0);
        QVERIFY(index.data(ProcessModel::MemoryRole).toULongLong() >= 0);
        const double cpu = index.data(ProcessModel::CpuRole).toDouble();
        QVERIFY(cpu >= 0.0 && cpu <= 100.0);
        if (index.data(ProcessModel::PidRole).toInt() == QCoreApplication::applicationPid()) {
            foundSelf = true;
            QVERIFY(index.data(ProcessModel::MemoryRole).toULongLong() > 0);
            QVERIFY(index.data(ProcessModel::NameRole).toString().isEmpty() == false);
        }
    }
    QVERIFY2(foundSelf, "the sampler did not report its own process");

    auto verifySorted = [model](bool byMemory, bool ascending) {
        for (int row = 1; row < model->rowCount(); ++row) {
            const QModelIndex previous = model->index(row - 1, 0);
            const QModelIndex current = model->index(row, 0);
            if (byMemory) {
                const quint64 a = previous.data(ProcessModel::MemoryRole).toULongLong();
                const quint64 b = current.data(ProcessModel::MemoryRole).toULongLong();
                if (ascending ? a > b : a < b) {
                    return false;
                }
            } else {
                const double a = previous.data(ProcessModel::CpuRole).toDouble();
                const double b = current.data(ProcessModel::CpuRole).toDouble();
                // CPU is sorted on the displayed 0.1% resolution, so rows that
                // show the same value may sit in either order (tie broken by
                // PID) without the list being out of order.
                if (ascending ? a > b + 0.2 : a < b - 0.2) {
                    return false;
                }
            }
        }
        return true;
    };

    backend.setSortKey(QStringLiteral("cpu"));
    QVERIFY(!backend.sortAscending());
    QVERIFY(verifySorted(false, false));

    backend.toggleSort(QStringLiteral("cpu"));
    QVERIFY(backend.sortAscending());
    QVERIFY(verifySorted(false, true));

    backend.toggleSort(QStringLiteral("memory"));
    QCOMPARE(backend.sortKey(), QStringLiteral("memory"));
    QVERIFY(!backend.sortAscending());
    QVERIFY(verifySorted(true, false));
}

void PerfTest::processListPause()
{
    PerfBackend backend;
    backend.setProcessListActive(true);
    QTRY_VERIFY_WITH_TIMEOUT(backend.processCount() > 0, 3000);

    QAbstractListModel *model = backend.processModel();
    // The table refreshes by diffing into the same model: a reset would destroy
    // every delegate and with it the row-move animation, so none is expected.
    QSignalSpy resetSpy(model, &QAbstractItemModel::modelReset);
    QSignalSpy changeSpy(model, &QAbstractItemModel::dataChanged);
    QSignalSpy countSpy(&backend, &PerfBackend::processCountChanged);
    QSignalSpy statsSpy(&backend, &PerfBackend::statsChanged);
    QVERIFY(!backend.processListPaused());

    backend.setProcessListPaused(true);
    QVERIFY(backend.processListPaused());
    const int frozenCount = backend.processCount();
    resetSpy.clear();
    changeSpy.clear();
    countSpy.clear();
    statsSpy.clear();

    QTest::qWait(1400);

    // The table is a still frame: no rescan, no resort, no row moves.
    QCOMPARE(changeSpy.count(), 0);
    QCOMPARE(countSpy.count(), 0);
    QCOMPARE(backend.processCount(), frozenCount);
    // The machine totals keep ticking while the table is frozen.
    QVERIFY2(statsSpy.count() >= 1, "CPU / memory stats must keep updating while paused");

    backend.setProcessListPaused(false);
    QVERIFY(!backend.processListPaused());
    // Resuming rescans into the existing rows instead of rebuilding the model.
    QVERIFY2(countSpy.count() >= 1, "resuming must rescan immediately");
    QCOMPARE(resetSpy.count(), 0);

    // Opening the popup clears a stale freeze, so a destroyed QML item can
    // never leave the list permanently stuck.
    backend.setProcessListPaused(true);
    backend.setProcessListActive(false);
    backend.setProcessListActive(true);
    QVERIFY(!backend.processListPaused());
}

void PerfTest::removeProcess()
{
    PerfBackend backend;
    backend.setProcessListActive(true);
    QTRY_VERIFY_WITH_TIMEOUT(backend.processCount() > 0, 3000);

    QAbstractListModel *model = backend.processModel();
    const int pid = model->index(0, 0).data(ProcessModel::PidRole).toInt();
    const int before = backend.processCount();
    QSignalSpy countSpy(&backend, &PerfBackend::processCountChanged);

    QVERIFY(backend.removeProcess(pid));
    QCOMPARE(backend.processCount(), before - 1);
    QCOMPARE(countSpy.count(), 1);
    for (int row = 0; row < model->rowCount(); ++row) {
        QVERIFY(model->index(row, 0).data(ProcessModel::PidRole).toInt() != pid);
    }

    QVERIFY(!backend.removeProcess(pid));
    QVERIFY(!backend.removeProcess(-1));
    QCOMPARE(backend.processCount(), before - 1);
}

void PerfTest::processSignals()
{
    PerfBackend backend;

    // PID 1 and non-positive PIDs are always refused.
    QVERIFY(!backend.terminateProcess(1).isEmpty());
    QVERIFY(!backend.killProcess(1).isEmpty());
    QVERIFY(!backend.terminateProcess(-5).isEmpty());

    // Force kill keeps DMS' PID > 1000 guard.
    QVERIFY(!backend.killProcess(500).isEmpty());

    // Nonexistent process -> ESRCH.
    QVERIFY(!backend.terminateProcess(999999999).isEmpty());
    QVERIFY(!backend.killProcess(999999999).isEmpty());

    QProcess termVictim;
    termVictim.start(QStringLiteral("sleep"), {QStringLiteral("30")});
    QVERIFY(termVictim.waitForStarted(3000));
    QCOMPARE(backend.terminateProcess(int(termVictim.processId())), QString());
    QVERIFY(termVictim.waitForFinished(3000));

    QProcess killVictim;
    killVictim.start(QStringLiteral("sleep"), {QStringLiteral("30")});
    QVERIFY(killVictim.waitForStarted(3000));
    QCOMPARE(backend.killProcess(int(killVictim.processId())), QString());
    QVERIFY(killVictim.waitForFinished(3000));
}

QTEST_GUILESS_MAIN(PerfTest)

#include "tests.moc"
