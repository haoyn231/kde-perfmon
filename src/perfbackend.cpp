#include "perfbackend.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QIcon>
#include <QStandardPaths>
#include <QSysInfo>
#include <QThread>
#include <QUrl>

#include <pwd.h>
#include <unistd.h>

namespace {

constexpr int kUpdateIntervalMs = 1000;

QString unquote(const QByteArray &value)
{
    QByteArray v = value.trimmed();
    if (v.size() >= 2 && v.startsWith('"') && v.endsWith('"')) {
        v = v.mid(1, v.size() - 2);
    }
    return QString::fromUtf8(v);
}

QHash<QString, QString> readOsRelease()
{
    QHash<QString, QString> values;
    QFile file(QStringLiteral("/etc/os-release"));
    if (!file.open(QIODevice::ReadOnly)) {
        file.setFileName(QStringLiteral("/usr/lib/os-release"));
        if (!file.open(QIODevice::ReadOnly)) {
            return values;
        }
    }
    const QList<QByteArray> lines = file.readAll().split('\n');
    for (const QByteArray &line : lines) {
        if (line.isEmpty() || line.startsWith('#')) {
            continue;
        }
        const int equals = line.indexOf('=');
        if (equals <= 0) {
            continue;
        }
        values.insert(QString::fromUtf8(line.left(equals)), unquote(line.mid(equals + 1)));
    }
    return values;
}

QString loginName()
{
    struct passwd pwd;
    struct passwd *result = nullptr;
    char buffer[4096];
    if (getpwuid_r(getuid(), &pwd, buffer, sizeof(buffer), &result) == 0 && result && result->pw_name) {
        return QString::fromLocal8Bit(result->pw_name);
    }
    const QByteArray user = qgetenv("USER");
    return user.isEmpty() ? QStringLiteral("user") : QString::fromLocal8Bit(user);
}

// Prefer a real distro logo file (Arch ships /usr/share/pixmaps/archlinux-logo.svg),
// then fall back to an icon-theme name. Returns the file path, or empty.
QString resolveDistroLogoPath(const QString &id, const QString &logoHint)
{
    QStringList names;
    if (!id.isEmpty()) {
        names << id + QStringLiteral("-logo");
    }
    if (!logoHint.isEmpty()) {
        names << logoHint;
    }
    const QStringList dirs = {
        QStringLiteral("/usr/share/pixmaps"),
        QStringLiteral("/usr/share/icons/hicolor/scalable/apps"),
    };
    for (const QString &dir : dirs) {
        for (const QString &name : names) {
            for (const QString &ext : {QStringLiteral(".svg"), QStringLiteral(".png")}) {
                const QString path = dir + QLatin1Char('/') + name + ext;
                if (QFileInfo::exists(path)) {
                    return path;
                }
            }
        }
    }
    return {};
}

QString resolveDistroLogoIcon(const QString &id, const QString &logoHint)
{
    QStringList names;
    if (!logoHint.isEmpty()) {
        names << logoHint;
    }
    if (!id.isEmpty()) {
        names << QStringLiteral("distributor-logo-") + id;
    }
    names << QStringLiteral("distributor-logo")
          << QStringLiteral("computer-symbolic")
          << QStringLiteral("computer");
    if (QGuiApplication::instance()) {
        for (const QString &name : names) {
            if (QIcon::hasThemeIcon(name)) {
                return name;
            }
        }
    }
    return QStringLiteral("computer-symbolic");
}

QString resolveAvatarPath(const QString &user)
{
    QStringList candidates;
    candidates << QStringLiteral("/var/lib/AccountsService/icons/") + user;
    const QString home = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
    if (!home.isEmpty()) {
        candidates << home + QStringLiteral("/.face")
                   << home + QStringLiteral("/.face.icon");
    }
    candidates << QStringLiteral("/usr/share/sddm/faces/") + user + QStringLiteral(".face.icon");

    for (const QString &path : candidates) {
        const QFileInfo info(path);
        if (info.isFile() && info.isReadable()) {
            return path;
        }
    }
    return {};
}

QString localFileUrl(const QString &path)
{
    return path.isEmpty() ? QString() : QUrl::fromLocalFile(path).toString();
}

} // namespace

PerfBackend::PerfBackend(QObject *parent)
    : QObject(parent)
{
    m_cpuCount = qMax(1, QThread::idealThreadCount());
    const long clockTicks = sysconf(_SC_CLK_TCK);
    m_clockTicks = clockTicks > 0 ? int(clockTicks) : 100;
    const long pageSize = sysconf(_SC_PAGESIZE);
    m_pageSize = pageSize > 0 ? quint64(pageSize) : 4096;

    const QHash<QString, QString> osRelease = readOsRelease();
    m_distroName = osRelease.value(QStringLiteral("NAME"));
    if (m_distroName.isEmpty()) {
        m_distroName = osRelease.value(QStringLiteral("PRETTY_NAME"));
    }
    if (m_distroName.isEmpty()) {
        m_distroName = QStringLiteral("Linux");
    }
    const QString distroId = osRelease.value(QStringLiteral("ID"));
    const QString logoHint = osRelease.value(QStringLiteral("LOGO"));
    m_distroLogoUrl = localFileUrl(resolveDistroLogoPath(distroId, logoHint));
    m_distroLogoIcon = resolveDistroLogoIcon(distroId, logoHint);

    m_hostName = QSysInfo::machineHostName();
    if (m_hostName.isEmpty()) {
        m_hostName = QStringLiteral("localhost");
    }
    m_userName = loginName();
    m_avatarUrl = localFileUrl(resolveAvatarPath(m_userName));

    m_clock.start();
    refresh(); // establishes the CPU baseline and fills memory totals

    m_timer.setInterval(kUpdateIntervalMs);
    m_timer.setTimerType(Qt::CoarseTimer);
    connect(&m_timer, &QTimer::timeout, this, &PerfBackend::refresh);
    m_timer.start();
}

double PerfBackend::memoryUsage() const
{
    if (m_memoryTotalBytes == 0) {
        return 0.0;
    }
    return 100.0 * double(m_memoryUsedBytes) / double(m_memoryTotalBytes);
}

int PerfBackend::processCount() const
{
    return m_model.rowCount();
}

void PerfBackend::setProcessListActive(bool active)
{
    if (m_processListActive == active) {
        return;
    }
    m_processListActive = active;
    if (active) {
        // Start a fresh delta window so the first visible values are current
        // instead of averaged over the time the popup was closed.
        m_previousTicks.clear();
        m_previousProcessSampleMs = -1;
        refresh();
    }
    Q_EMIT processListActiveChanged();
}

void PerfBackend::setSortKey(const QString &key)
{
    if (key != QLatin1String("cpu") && key != QLatin1String("memory")) {
        return;
    }
    if (m_sortKey == key) {
        return;
    }
    m_sortKey = key;
    m_sortAscending = false;
    resort();
    Q_EMIT sortChanged();
}

void PerfBackend::toggleSort(const QString &key)
{
    if (key != QLatin1String("cpu") && key != QLatin1String("memory")) {
        return;
    }
    if (m_sortKey == key) {
        m_sortAscending = !m_sortAscending;
    } else {
        m_sortKey = key;
        m_sortAscending = false;
    }
    resort();
    Q_EMIT sortChanged();
}

void PerfBackend::resort()
{
    if (!m_processListActive) {
        return;
    }
    // Reuse the current rows; sorting again is cheap compared to rescanning.
    QVector<ProcessInfo> processes;
    processes.reserve(m_model.rowCount());
    for (int row = 0; row < m_model.rowCount(); ++row) {
        const QModelIndex index = m_model.index(row, 0);
        ProcessInfo info;
        info.pid = index.data(ProcessModel::PidRole).toInt();
        info.ppid = index.data(ProcessModel::PpidRole).toInt();
        info.name = index.data(ProcessModel::NameRole).toString();
        info.command = index.data(ProcessModel::CommandRole).toString();
        info.cpu = index.data(ProcessModel::CpuRole).toDouble();
        info.memoryBytes = index.data(ProcessModel::MemoryRole).toULongLong();
        info.memoryPercent = index.data(ProcessModel::MemoryPercentRole).toDouble();
        processes.append(info);
    }
    sortProcesses(processes, m_sortKey, m_sortAscending);
    m_model.setProcesses(std::move(processes));
}

void PerfBackend::refresh()
{
    const qint64 nowMs = m_clock.elapsed();

    CpuTimes currentCpu;
    QFile statFile(QStringLiteral("/proc/stat"));
    if (statFile.open(QIODevice::ReadOnly) && parseCpuTimesLine(statFile.readLine(), &currentCpu)) {
        if (m_havePreviousCpu) {
            const quint64 deltaTotal = currentCpu.total() - m_previousCpu.total();
            const quint64 deltaIdle = currentCpu.idleAll() - m_previousCpu.idleAll();
            if (deltaTotal > 0) {
                const quint64 deltaBusy = deltaTotal > deltaIdle ? deltaTotal - deltaIdle : 0;
                m_cpuUsage = 100.0 * double(deltaBusy) / double(deltaTotal);
            }
        }
        m_previousCpu = currentCpu;
        m_havePreviousCpu = true;
    }

    readMemory();

    if (m_processListActive) {
        scanProcesses(nowMs);
    }

    Q_EMIT statsChanged();
}

void PerfBackend::readMemory()
{
    QFile file(QStringLiteral("/proc/meminfo"));
    if (!file.open(QIODevice::ReadOnly)) {
        return;
    }

    qulonglong totalKb = 0;
    qulonglong availableKb = 0;
    qulonglong freeKb = 0;
    qulonglong buffersKb = 0;
    qulonglong cachedKb = 0;
    qulonglong reclaimableKb = 0;
    qulonglong shmemKb = 0;
    qulonglong swapTotalKb = 0;
    qulonglong swapFreeKb = 0;
    bool haveAvailable = false;

    const QList<QByteArray> lines = file.readAll().split('\n');
    for (const QByteArray &line : lines) {
        const int colon = line.indexOf(':');
        if (colon < 0) {
            continue;
        }
        const QByteArray key = line.left(colon);
        bool ok = false;
        const qulonglong value = line.mid(colon + 1).trimmed().split(' ').first().toULongLong(&ok);
        if (!ok) {
            continue;
        }
        if (key == "MemTotal") {
            totalKb = value;
        } else if (key == "MemAvailable") {
            availableKb = value;
            haveAvailable = true;
        } else if (key == "MemFree") {
            freeKb = value;
        } else if (key == "Buffers") {
            buffersKb = value;
        } else if (key == "Cached") {
            cachedKb = value;
        } else if (key == "SReclaimable") {
            reclaimableKb = value;
        } else if (key == "Shmem") {
            shmemKb = value;
        } else if (key == "SwapTotal") {
            swapTotalKb = value;
        } else if (key == "SwapFree") {
            swapFreeKb = value;
        }
    }

    // dgop/gopsutil semantics (what DMS shows): total - free - buffers -
    // (Cached + SReclaimable - Shmem). MemAvailable is the fallback for kernels
    // where one of those fields is unavailable.
    const qulonglong total = totalKb * 1024;
    qulonglong reclaimedKb = cachedKb + reclaimableKb;
    reclaimedKb = reclaimedKb > shmemKb ? reclaimedKb - shmemKb : 0;
    const qulonglong usedKb = totalKb > freeKb + buffersKb + reclaimedKb
        ? totalKb - freeKb - buffersKb - reclaimedKb
        : 0;
    const qulonglong available = (haveAvailable ? availableKb : freeKb) * 1024;
    m_memoryTotalBytes = total;
    m_memoryUsedBytes = (buffersKb > 0 || cachedKb > 0) ? usedKb * 1024 : (total > available ? total - available : 0);
    m_swapTotalBytes = swapTotalKb * 1024;
    m_swapUsedBytes = swapTotalKb > swapFreeKb ? (swapTotalKb - swapFreeKb) * 1024 : 0;
}

void PerfBackend::scanProcesses(qint64 nowMs)
{
    const bool havePrevious = m_previousProcessSampleMs >= 0;
    const qint64 deltaMs = havePrevious ? nowMs - m_previousProcessSampleMs : 0;

    QVector<ProcessInfo> processes;
    processes.reserve(1024);
    QHash<int, quint64> currentTicks;
    currentTicks.reserve(1024);

    const QDir procDir(QStringLiteral("/proc"));
    const QStringList entries = procDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString &entry : entries) {
        bool pidOk = false;
        const int pid = entry.toInt(&pidOk);
        if (!pidOk) {
            continue;
        }

        QFile statFile(procDir.filePath(entry + QStringLiteral("/stat")));
        if (!statFile.open(QIODevice::ReadOnly)) {
            continue;
        }
        ProcStat stat;
        if (!parseProcStat(statFile.readAll(), &stat, m_pageSize)) {
            continue;
        }

        QString command;
        QFile cmdlineFile(procDir.filePath(entry + QStringLiteral("/cmdline")));
        if (cmdlineFile.open(QIODevice::ReadOnly)) {
            command = parseCmdline(cmdlineFile.readAll());
        }
        if (command.isEmpty()) {
            command = stat.name;
        }

        ProcessInfo info;
        info.pid = pid;
        info.ppid = stat.ppid;
        info.name = stat.name;
        info.command = command;
        info.memoryBytes = stat.rssBytes;
        info.memoryPercent = m_memoryTotalBytes > 0
            ? 100.0 * double(stat.rssBytes) / double(m_memoryTotalBytes)
            : 0.0;

        if (havePrevious) {
            const auto previous = m_previousTicks.constFind(pid);
            if (previous != m_previousTicks.constEnd() && stat.cpuTicks >= previous.value()) {
                info.cpu = processCpuPercent(stat.cpuTicks - previous.value(), deltaMs, m_clockTicks, m_cpuCount);
            }
        }

        processes.append(info);
        currentTicks.insert(pid, stat.cpuTicks);
    }

    m_previousTicks = std::move(currentTicks);
    m_previousProcessSampleMs = nowMs;

    sortProcesses(processes, m_sortKey, m_sortAscending);
    m_model.setProcesses(std::move(processes));
    Q_EMIT processCountChanged();
}
