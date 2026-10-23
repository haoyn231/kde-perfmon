#pragma once

#include "processmodel.h"

#include <QElapsedTimer>
#include <QHash>
#include <QObject>
#include <QTimer>

class PerfBackend : public QObject
{
    Q_OBJECT

    Q_PROPERTY(double cpuUsage READ cpuUsage NOTIFY statsChanged)
    Q_PROPERTY(double memoryUsage READ memoryUsage NOTIFY statsChanged)
    Q_PROPERTY(qulonglong memoryUsedBytes READ memoryUsedBytes NOTIFY statsChanged)
    Q_PROPERTY(qulonglong memoryTotalBytes READ memoryTotalBytes NOTIFY statsChanged)
    Q_PROPERTY(qulonglong swapUsedBytes READ swapUsedBytes NOTIFY statsChanged)
    Q_PROPERTY(qulonglong swapTotalBytes READ swapTotalBytes NOTIFY statsChanged)
    Q_PROPERTY(int processCount READ processCount NOTIFY processCountChanged)
    Q_PROPERTY(int cpuCount READ cpuCount CONSTANT)
    Q_PROPERTY(QAbstractListModel *processModel READ processModel CONSTANT)
    Q_PROPERTY(bool processListActive READ processListActive WRITE setProcessListActive NOTIFY processListActiveChanged)
    Q_PROPERTY(QString sortKey READ sortKey NOTIFY sortChanged)
    Q_PROPERTY(bool sortAscending READ sortAscending NOTIFY sortChanged)
    Q_PROPERTY(QString distroName READ distroName CONSTANT)
    Q_PROPERTY(QString distroLogoUrl READ distroLogoUrl CONSTANT)
    Q_PROPERTY(QString distroLogoIcon READ distroLogoIcon CONSTANT)
    Q_PROPERTY(QString hostName READ hostName CONSTANT)
    Q_PROPERTY(QString userName READ userName CONSTANT)
    Q_PROPERTY(QString avatarUrl READ avatarUrl CONSTANT)

public:
    explicit PerfBackend(QObject *parent = nullptr);

    double cpuUsage() const { return m_cpuUsage; }
    double memoryUsage() const;
    qulonglong memoryUsedBytes() const { return m_memoryUsedBytes; }
    qulonglong memoryTotalBytes() const { return m_memoryTotalBytes; }
    qulonglong swapUsedBytes() const { return m_swapUsedBytes; }
    qulonglong swapTotalBytes() const { return m_swapTotalBytes; }
    int processCount() const;
    int cpuCount() const { return m_cpuCount; }

    QAbstractListModel *processModel() { return &m_model; }

    bool processListActive() const { return m_processListActive; }
    void setProcessListActive(bool active);

    QString sortKey() const { return m_sortKey; }
    bool sortAscending() const { return m_sortAscending; }

    QString distroName() const { return m_distroName; }
    QString distroLogoUrl() const { return m_distroLogoUrl; }
    QString distroLogoIcon() const { return m_distroLogoIcon; }
    QString hostName() const { return m_hostName; }
    QString userName() const { return m_userName; }
    QString avatarUrl() const { return m_avatarUrl; }

    // DMS semantics: picking a different column starts descending, picking the
    // active column flips the direction.
    Q_INVOKABLE void setSortKey(const QString &key);
    Q_INVOKABLE void toggleSort(const QString &key);

public Q_SLOTS:
    void refresh();

Q_SIGNALS:
    void statsChanged();
    void processCountChanged();
    void processListActiveChanged();
    void sortChanged();

private:
    void readMemory();
    void scanProcesses(qint64 nowMs);
    void resort();

    ProcessModel m_model;
    QTimer m_timer;
    QElapsedTimer m_clock;

    CpuTimes m_previousCpu;
    bool m_havePreviousCpu = false;
    QHash<int, quint64> m_previousTicks;
    qint64 m_previousProcessSampleMs = -1;

    double m_cpuUsage = 0.0;
    qulonglong m_memoryUsedBytes = 0;
    qulonglong m_memoryTotalBytes = 0;
    qulonglong m_swapUsedBytes = 0;
    qulonglong m_swapTotalBytes = 0;

    QString m_distroName;
    QString m_distroLogoUrl;
    QString m_distroLogoIcon;
    QString m_hostName;
    QString m_userName;
    QString m_avatarUrl;

    QString m_sortKey = QStringLiteral("cpu");
    bool m_sortAscending = false;
    bool m_processListActive = false;

    int m_cpuCount = 1;
    int m_clockTicks = 100;
    quint64 m_pageSize = 4096;
};
