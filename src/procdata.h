#pragma once

#include <QByteArray>
#include <QString>
#include <QVector>

struct CpuTimes {
    quint64 user = 0;
    quint64 nice = 0;
    quint64 system = 0;
    quint64 idle = 0;
    quint64 iowait = 0;
    quint64 irq = 0;
    quint64 softirq = 0;
    quint64 steal = 0;

    quint64 total() const;
    quint64 idleAll() const;
};

// Parses the aggregate "/proc/stat" line ("cpu  user nice system ...").
bool parseCpuTimesLine(const QByteArray &line, CpuTimes *out);

// Parses one per-thread "/proc/stat" line ("cpu0  user nice system ...") and
// reports the thread index in *index. The aggregate line is rejected on
// purpose; parseCpuTimesLine() is the one that handles it.
bool parseCoreCpuTimesLine(const QByteArray &line, int *index, CpuTimes *out);

// Busy share (0-100) between two samples of the same counter set. Returns 0
// when the counters did not advance (e.g. the sample window was too short).
double cpuBusyPercent(const CpuTimes &previous, const CpuTimes &current);

// Counts the physical cores described by /proc/cpuinfo (unique
// "physical id" + "core id" pairs). Returns 0 when the file carries no
// topology information at all, so callers can fall back to the thread count.
int countPhysicalCores(const QByteArray &cpuInfoData);

// RAPL / hwmon energy counter delta -> average watts over the sample window.
// `maxRangeUj` is the counter's wrap value (0 when unknown); a counter that
// went backwards without a known range is treated as a reset and reported as 0.
double raplPowerWatts(quint64 previousUj, quint64 currentUj, quint64 maxRangeUj, qint64 deltaNs);

struct ProcStat {
    int pid = -1;
    int ppid = -1;
    QString name;
    quint64 cpuTicks = 0; // utime + stime
    quint64 rssBytes = 0;
};

// Parses "/proc/<pid>/stat". `name` may contain spaces and parentheses.
bool parseProcStat(const QByteArray &data, ProcStat *out, quint64 pageSize);

// Parses "/proc/<pid>/cmdline" (NUL separated) into a single display string.
QString parseCmdline(const QByteArray &data);

struct ProcessInfo {
    int pid = 0;
    int ppid = 0;
    QString name;
    QString command;
    double cpu = 0.0; // percent of the whole machine (0-100), dgop/DMS semantics
    quint64 memoryBytes = 0;
    double memoryPercent = 0.0;
};

// CPU share of one process relative to all cores; top-style core percentage
// divided by the core count, clamped to 100 like dgop does.
double processCpuPercent(quint64 deltaTicks, qint64 deltaMs, int clockTicks, int cpuCount);

// Sorts in place. Descending is the default in DMS; pid is the ascending tie break.
void sortProcesses(QVector<ProcessInfo> &processes, const QString &key, bool ascending);
