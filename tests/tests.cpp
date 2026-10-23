#include "perfbackend.h"
#include "procdata.h"

#include <QCoreApplication>
#include <QFileInfo>
#include <QUrl>
#include <QtTest>

class PerfTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void parseCpuTimes();
    void rejectForeignCpuLine();
    void parseProcStatWithParens();
    void cmdlineParsing();
    void processCpuMath();
    void sortByCpuAndMemory();
    void liveSampling();
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
    QVERIFY(backend.memoryTotalBytes() > 0);
    QVERIFY(backend.memoryUsedBytes() <= backend.memoryTotalBytes());

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
                if (ascending ? a > b : a < b) {
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

QTEST_GUILESS_MAIN(PerfTest)

#include "tests.moc"
