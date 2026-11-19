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

#include <cerrno>
#include <cmath>
#include <csignal>
#include <cstring>
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

QString signalProcess(int pid, int signalNumber, bool force)
{
    if (pid <= 1) {
        return QStringLiteral("Refusing to signal PID %1").arg(pid);
    }
    if (force && pid <= 1000) {
        return QStringLiteral("Refusing to SIGKILL PID %1 (only PIDs above 1000)").arg(pid);
    }
    if (::kill(pid, signalNumber) != 0) {
        return QString::fromLocal8Bit(std::strerror(errno));
    }
    return {};
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

    // Physical cores are only a nicety for the card subtitle: hybrid parts
    // report a different thread count, and the per-core strip always follows
    // the cpuN lines of /proc/stat.
    QFile cpuInfo(QStringLiteral("/proc/cpuinfo"));
    if (cpuInfo.open(QIODevice::ReadOnly)) {
        m_physicalCpuCount = countPhysicalCores(cpuInfo.readAll());
    }

    resolvePowerSource();

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
        // A fresh popup always starts pinned to the top, so it starts live even
        // if the QML item was destroyed while the table was frozen.
        if (m_processListPaused) {
            m_processListPaused = false;
            Q_EMIT processListPausedChanged();
        }
        refresh();
    }
    Q_EMIT processListActiveChanged();
}

void PerfBackend::setProcessListPaused(bool paused)
{
    if (m_processListPaused == paused) {
        return;
    }
    m_processListPaused = paused;
    const bool resuming = !paused && m_processListActive;
    if (resuming) {
        // Resume from a fresh delta window: ticks accumulated while frozen would
        // otherwise be reported as CPU usage averaged over the whole pause.
        m_previousTicks.clear();
        m_previousProcessSampleMs = -1;
    }
    // Notify before rescanning, so the UI is out of its "paused" state by the
    // time the rows change.
    Q_EMIT processListPausedChanged();
    if (resuming) {
        refresh();
    }
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

QString PerfBackend::terminateProcess(int pid)
{
    return signalProcess(pid, SIGTERM, false);
}

QString PerfBackend::killProcess(int pid)
{
    return signalProcess(pid, SIGKILL, true);
}

bool PerfBackend::removeProcess(int pid)
{
    if (!m_model.removePid(pid)) {
        return false;
    }
    Q_EMIT processCountChanged();
    return true;
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

    // One pass over /proc/stat gives both the machine total and every cpuN
    // line, so the per-core strip costs nothing extra.
    QVector<CpuTimes> cores;
    CpuTimes currentCpu;
    bool haveAggregate = false;
    QFile statFile(QStringLiteral("/proc/stat"));
    if (statFile.open(QIODevice::ReadOnly)) {
        // QFile::atEnd() lies for /proc files (size() is 0), so read until an
        // empty line comes back instead.
        while (true) {
            const QByteArray line = statFile.readLine();
            if (line.isEmpty()) {
                break;
            }
            CpuTimes times;
            if (line.startsWith("cpu ")) {
                if (parseCpuTimesLine(line, &times)) {
                    currentCpu = times;
                    haveAggregate = true;
                }
                continue;
            }
            int index = -1;
            if (parseCoreCpuTimesLine(line, &index, &times)) {
                if (cores.size() <= index) {
                    cores.resize(index + 1);
                }
                cores[index] = times;
            }
        }
    }
    if (haveAggregate) {
        if (m_havePreviousCpu) {
            m_cpuUsage = cpuBusyPercent(m_previousCpu, currentCpu);
        }
        m_previousCpu = currentCpu;
        m_havePreviousCpu = true;
    }
    readCoreUsage(cores);
    readCpuPower(nowMs);

    readMemory();

    // A paused table keeps its rows exactly as they are: rescans would resort
    // the model and yank the view out from under the reader.
    if (m_processListActive && !m_processListPaused) {
        scanProcesses(nowMs);
    }

    Q_EMIT statsChanged();
}

void PerfBackend::readCoreUsage(const QVector<CpuTimes> &cores)
{
    if (cores.isEmpty()) {
        // No cpuN lines at all (very old kernel, or a container with a masked
        // /proc/stat): drop the strip instead of drawing zeros.
        m_previousCores.clear();
        if (!m_coreUsage.isEmpty()) {
            m_coreUsage.clear();
            Q_EMIT coreUsageChanged();
        }
        return;
    }

    // First frame, or the thread count changed (SMT toggled, CPU hotplug):
    // take the counters as the new baseline without publishing percentages,
    // otherwise the first frame would be averaged over an unknown window.
    if (m_previousCores.size() != cores.size()) {
        m_previousCores = cores;
        if (!m_coreUsage.isEmpty() && m_coreUsage.size() != cores.size()) {
            m_coreUsage.clear();
            Q_EMIT coreUsageChanged();
        }
        return;
    }

    QVariantList usage;
    usage.reserve(cores.size());
    bool changed = m_coreUsage.size() != cores.size();
    for (int thread = 0; thread < cores.size(); ++thread) {
        const double percent = std::round(cpuBusyPercent(m_previousCores.at(thread), cores.at(thread)) * 10.0) / 10.0;
        usage.append(percent);
        if (!changed && std::abs(m_coreUsage.at(thread).toDouble() - percent) >= 0.5) {
            changed = true;
        }
    }
    m_previousCores = cores;

    if (!changed) {
        return; // sub-percent jitter is not worth repainting 24 bars
    }
    m_coreUsage = usage;
    Q_EMIT coreUsageChanged();
}

void PerfBackend::resolvePowerSource()
{
    // RAPL first: it is the only source that reports the whole package,
    // including the uncore, and it is world readable on most kernels.
    const QDir powercap(QStringLiteral("/sys/class/powercap"));
    const QStringList zones = powercap.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    QString packageZone;
    for (const QString &zone : zones) {
        if (!zone.contains(QLatin1String("rapl"))) {
            continue;
        }
        QFile nameFile(powercap.filePath(zone + QStringLiteral("/name")));
        if (!nameFile.open(QIODevice::ReadOnly)) {
            continue;
        }
        const QString name = QString::fromUtf8(nameFile.readAll()).trimmed();
        if (!name.startsWith(QLatin1String("package"))) {
            continue;
        }
        if (!QFileInfo(powercap.filePath(zone + QStringLiteral("/energy_uj"))).isReadable()) {
            continue;
        }
        // intel-rapl-mmio mirrors the same package through MMIO; prefer the
        // MSR-backed zone when both are present.
        if (packageZone.isEmpty()
            || (packageZone.contains(QLatin1String("mmio")) && !zone.contains(QLatin1String("mmio")))) {
            packageZone = zone;
        }
    }
    if (!packageZone.isEmpty()) {
        m_powerSource = PowerSource::EnergyCounter;
        m_powerPath = powercap.filePath(packageZone + QStringLiteral("/energy_uj"));
        const QString rangePath = powercap.filePath(packageZone + QStringLiteral("/max_energy_range_uj"));
        if (QFileInfo(rangePath).isReadable()) {
            m_powerRangePath = rangePath;
        }
        return;
    }

    // hwmon fallback for machines where RAPL is not exposed (AMD with
    // zenpower / amd_energy). Only CPU drivers are accepted: amdgpu and nvme
    // also publish power1_average, and those are not the CPU package.
    const QStringList cpuDrivers = {
        QStringLiteral("zenpower"),
        QStringLiteral("amd_energy"),
        QStringLiteral("k10temp"),
        QStringLiteral("coretemp"),
        QStringLiteral("cpu"),
    };
    const QDir hwmon(QStringLiteral("/sys/class/hwmon"));
    const QStringList sensors = hwmon.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    for (int pass = 0; pass < 2; ++pass) {
        const QString leaf = pass == 0 ? QStringLiteral("/power1_average") : QStringLiteral("/energy1_input");
        for (const QString &sensor : sensors) {
            const QString base = hwmon.filePath(sensor);
            QFile nameFile(base + QStringLiteral("/name"));
            if (!nameFile.open(QIODevice::ReadOnly)) {
                continue;
            }
            const QString name = QString::fromUtf8(nameFile.readAll()).trimmed();
            if (!cpuDrivers.contains(name)) {
                continue;
            }
            const QString path = base + leaf;
            if (!QFileInfo(path).isReadable()) {
                continue;
            }
            m_powerSource = pass == 0 ? PowerSource::PowerAverage : PowerSource::EnergyCounter;
            m_powerPath = path;
            return;
        }
    }
}

void PerfBackend::readCpuPower(qint64 nowMs)
{
    if (m_powerSource == PowerSource::None) {
        return;
    }

    const auto readCounter = [](const QString &path, quint64 *out) {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) {
            return false;
        }
        bool ok = false;
        const quint64 value = file.readAll().trimmed().toULongLong(&ok);
        if (!ok) {
            return false;
        }
        *out = value;
        return true;
    };

    if (m_powerSource == PowerSource::PowerAverage) {
        quint64 microWatts = 0;
        if (readCounter(m_powerPath, &microWatts) && microWatts > 0) {
            m_cpuPowerWatts = double(microWatts) / 1000000.0;
            m_cpuPowerAvailable = true;
        }
        return;
    }

    quint64 energyUj = 0;
    if (!readCounter(m_powerPath, &energyUj)) {
        return;
    }
    if (m_powerMaxRangeUj == 0 && !m_powerRangePath.isEmpty()) {
        quint64 range = 0;
        if (readCounter(m_powerRangePath, &range)) {
            m_powerMaxRangeUj = range;
        }
    }

    const qint64 elapsedMs = m_previousEnergyMs >= 0 ? nowMs - m_previousEnergyMs : -1;
    if (elapsedMs > 0) {
        const double watts = raplPowerWatts(m_previousEnergyUj, energyUj, m_powerMaxRangeUj, elapsedMs * 1000000LL);
        if (watts > 0.0) {
            // Sampled once a second, so a light EMA keeps the figure from
            // jumping a few watts between frames without hiding real changes.
            m_cpuPowerWatts = m_cpuPowerAvailable ? m_cpuPowerWatts * 0.4 + watts * 0.6 : watts;
            m_cpuPowerAvailable = true;
        }
    }
    m_previousEnergyUj = energyUj;
    m_previousEnergyMs = nowMs;
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
