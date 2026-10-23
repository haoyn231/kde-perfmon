#include "procdata.h"

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
    std::stable_sort(processes.begin(), processes.end(), [byMemory, ascending](const ProcessInfo &a, const ProcessInfo &b) {
        int cmp = 0;
        if (byMemory) {
            if (a.memoryBytes != b.memoryBytes) {
                cmp = a.memoryBytes < b.memoryBytes ? -1 : 1;
            }
        } else if (a.cpu != b.cpu) {
            cmp = a.cpu < b.cpu ? -1 : 1;
        }
        if (cmp == 0) {
            return a.pid < b.pid; // stable tie break, also like DMS
        }
        return ascending ? cmp < 0 : cmp > 0;
    });
}
