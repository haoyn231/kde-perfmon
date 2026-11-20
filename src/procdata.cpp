#include "procdata.h"

#include <QPair>
#include <QSet>

#include <algorithm>

quint64 CpuTimes::total() const
{
    // guest/guest_nice are already accounted for in user/nice.
    return user + nice + system + idle + iowait + irq + softirq + steal;
}

quint64 CpuTimes::idleAll() const
{
    return idle + iowait;
}

bool parseCpuTimesLine(const QByteArray &line, CpuTimes *out)
{
    if (!out || !line.startsWith("cpu ")) {
        return false;
    }
    const QList<QByteArray> fields = line.mid(4).simplified().split(' ');
    if (fields.size() < 8) {
        return false;
    }
    auto value = [&fields](int index) {
        bool ok = false;
        const quint64 v = fields.at(index).toULongLong(&ok);
        return ok ? v : quint64(0);
    };
    out->user = value(0);
    out->nice = value(1);
    out->system = value(2);
    out->idle = value(3);
    out->iowait = value(4);
    out->irq = value(5);
    out->softirq = value(6);
    out->steal = value(7);
    return true;
}

bool parseCoreCpuTimesLine(const QByteArray &line, int *index, CpuTimes *out)
{
    if (!out || !line.startsWith("cpu")) {
        return false;
    }
    int cursor = 3;
    while (cursor < line.size() && line.at(cursor) >= '0' && line.at(cursor) <= '9') {
        ++cursor;
    }
    // No digits ("cpu  ...") or no separating space ("cpu0x") -> not a core line.
    if (cursor == 3 || cursor >= line.size() || line.at(cursor) != ' ') {
        return false;
    }
    bool ok = false;
    const int core = line.mid(3, cursor - 3).toInt(&ok);
    if (!ok || core < 0) {
        return false;
    }
    // parseCpuTimesLine() only knows the aggregate form, so hand it the same
    // field list under a normalised label instead of duplicating the parsing.
    if (!parseCpuTimesLine("cpu" + line.mid(cursor), out)) {
        return false;
    }
    if (index) {
        *index = core;
    }
    return true;
}

double cpuBusyPercent(const CpuTimes &previous, const CpuTimes &current)
{
    const quint64 total = current.total();
    const quint64 previousTotal = previous.total();
    if (total <= previousTotal) {
        return 0.0;
    }
    const quint64 deltaTotal = total - previousTotal;
    const quint64 idle = current.idleAll();
    const quint64 deltaIdle = idle >= previous.idleAll() ? idle - previous.idleAll() : 0;
    const quint64 deltaBusy = deltaTotal > deltaIdle ? deltaTotal - deltaIdle : 0;
    return std::clamp(100.0 * double(deltaBusy) / double(deltaTotal), 0.0, 100.0);
}

int countPhysicalCores(const QByteArray &cpuInfoData)
{
    // One QSet entry per (physical id, core id) pair; a missing "core id" means
    // the kernel did not export topology and the caller should fall back.
    QSet<QPair<int, int>> cores;
    const QList<QByteArray> lines = cpuInfoData.split('\n');
    int currentCoreId = -1;
    int currentPhysicalId = 0;
    bool sawCoreId = false;

    auto flush = [&cores, &currentCoreId, &currentPhysicalId]() {
        if (currentCoreId >= 0) {
            cores.insert(qMakePair(currentPhysicalId, currentCoreId));
            currentCoreId = -1;
            currentPhysicalId = 0;
        }
    };

    for (const QByteArray &line : lines) {
        const int colon = line.indexOf(':');
        if (colon <= 0) {
            if (line.trimmed().isEmpty()) {
                flush();
            }
            continue;
        }
        const QByteArray key = line.left(colon).trimmed();
        const QByteArray value = line.mid(colon + 1).trimmed();
        if (key == "processor") {
            flush();
        } else if (key == "core id") {
            currentCoreId = value.toInt();
            sawCoreId = true;
        } else if (key == "physical id") {
            currentPhysicalId = value.toInt();
        }
    }
    flush();

    return sawCoreId ? cores.size() : 0;
}

double raplPowerWatts(quint64 previousUj, quint64 currentUj, quint64 maxRangeUj, qint64 deltaNs)
{
    if (deltaNs <= 0) {
        return 0.0;
    }
    quint64 deltaUj = 0;
    if (currentUj >= previousUj) {
        deltaUj = currentUj - previousUj;
    } else if (maxRangeUj > previousUj) {
        deltaUj = (maxRangeUj - previousUj) + currentUj; // counter wrapped around
    } else {
        return 0.0; // counter reset with no known range: this sample is garbage
    }
    const double watts = double(deltaUj) * 1000.0 / double(deltaNs); // uJ / ns == mW
    if (!(watts >= 0.0) || watts > 100000.0) {
        return 0.0;
    }
    return watts;
}

bool parseProcStat(const QByteArray &data, ProcStat *out, quint64 pageSize)
{
    if (!out) {
        return false;
    }
    const int open = data.indexOf('(');
    const int close = data.lastIndexOf(')');
    if (open < 0 || close <= open) {
        return false;
    }

    bool pidOk = false;
    const int pid = data.left(open).trimmed().toInt(&pidOk);
    if (!pidOk) {
        return false;
    }

    const QList<QByteArray> fields = data.mid(close + 2).simplified().split(' ');
    if (fields.size() < 22) {
        return false;
    }

    // Field 3 (state) is index 0 here, so utime/stime/rss are offset by 3.
    bool ppidOk = false;
    const int ppid = fields.at(1).toInt(&ppidOk);
    if (!ppidOk) {
        return false;
    }
    bool utimeOk = false;
    bool stimeOk = false;
    bool rssOk = false;
    const quint64 utime = fields.at(11).toULongLong(&utimeOk);
    const quint64 stime = fields.at(12).toULongLong(&stimeOk);
    const quint64 rssPages = fields.at(21).toULongLong(&rssOk);
    if (!utimeOk || !stimeOk || !rssOk) {
        return false;
    }

    out->pid = pid;
    out->ppid = ppid;
    out->name = QString::fromUtf8(data.mid(open + 1, close - open - 1));
    out->cpuTicks = utime + stime;
    out->rssBytes = rssPages * pageSize;
    return true;
}

QString parseCmdline(const QByteArray &data)
{
    QByteArray copy = data;
    copy.replace('\0', ' ');
    return QString::fromUtf8(copy).trimmed();
}

double processCpuPercent(quint64 deltaTicks, qint64 deltaMs, int clockTicks, int cpuCount)
{
    if (deltaTicks == 0 || deltaMs <= 0 || clockTicks <= 0) {
        return 0.0;
    }
    if (cpuCount <= 0) {
        cpuCount = 1;
    }
    const double corePercent = double(deltaTicks) * 100000.0 / (double(deltaMs) * double(clockTicks));
    return std::clamp(corePercent / double(cpuCount), 0.0, 100.0);
}

void sortProcesses(QVector<ProcessInfo> &processes, const QString &key, bool ascending)
{
    const bool byMemory = key == QLatin1String("memory");
    // CPU is compared at the resolution the table actually shows (one decimal).
    // The raw percentage carries far more digits, and the sub-0.1% noise of
    // sleeping processes is enough to reshuffle the whole idle tail on every
    // sample - which used to be invisible behind a full model rebuild, but now
    // would drag hundreds of rows around once a second. Quantising keeps those
    // rows ordered by PID between ticks, so a row only moves when it really
    // changed.
    auto cpuTenths = [](double percent) {
        return qRound64(percent * 10.0);
    };
    std::stable_sort(processes.begin(), processes.end(), [byMemory, ascending, cpuTenths](const ProcessInfo &a, const ProcessInfo &b) {
        int cmp = 0;
        if (byMemory) {
            if (a.memoryBytes != b.memoryBytes) {
                cmp = a.memoryBytes < b.memoryBytes ? -1 : 1;
            }
        } else {
            const qint64 aTenths = cpuTenths(a.cpu);
            const qint64 bTenths = cpuTenths(b.cpu);
            if (aTenths != bTenths) {
                cmp = aTenths < bTenths ? -1 : 1;
            }
        }
        if (cmp == 0) {
            return a.pid < b.pid; // stable tie break, also like DMS
        }
        return ascending ? cmp < 0 : cmp > 0;
    });
}
