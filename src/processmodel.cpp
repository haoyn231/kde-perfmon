#include "processmodel.h"

#include <utility>

ProcessModel::ProcessModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int ProcessModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_processes.size();
}

QVariant ProcessModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_processes.size()) {
        return {};
    }
    const ProcessInfo &process = m_processes.at(index.row());
    switch (role) {
    case PidRole:
        return process.pid;
    case PpidRole:
        return process.ppid;
    case NameRole:
        return process.name;
    case CommandRole:
        return process.command;
    case CpuRole:
        return process.cpu;
    case MemoryRole:
        return QVariant::fromValue(process.memoryBytes);
    case MemoryPercentRole:
        return process.memoryPercent;
    default:
        return {};
    }
}

QHash<int, QByteArray> ProcessModel::roleNames() const
{
    return {
        {PidRole, "pid"},
        {PpidRole, "ppid"},
        {NameRole, "name"},
        {CommandRole, "command"},
        {CpuRole, "cpu"},
        {MemoryRole, "memory"},
        {MemoryPercentRole, "memoryPercent"},
    };
}

void ProcessModel::setProcesses(QVector<ProcessInfo> processes)
{
    beginResetModel();
    m_processes = std::move(processes);
    endResetModel();
}
