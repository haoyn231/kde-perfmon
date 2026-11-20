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

void ProcessModel::syncProcesses(QVector<ProcessInfo> processes)
{
    // 1) Line the rows up with the new order. A row that is already in place is
    // left completely alone; a row that exists further down is moved instead of
    // being destroyed and re-created, which is what lets the view animate it.
    for (int target = 0; target < processes.size(); ++target) {
        if (target >= m_processes.size()) {
            beginInsertRows(QModelIndex(), target, target);
            m_processes.insert(target, processes.at(target));
            endInsertRows();
            continue;
        }
        const int pid = processes.at(target).pid;
        if (m_processes.at(target).pid == pid) {
            continue;
        }
        int existing = -1;
        for (int row = target + 1; row < m_processes.size(); ++row) {
            if (m_processes.at(row).pid == pid) {
                existing = row;
                break;
            }
        }
        if (existing >= 0) {
            beginMoveRows(QModelIndex(), existing, existing, QModelIndex(), target);
            m_processes.move(existing, target);
            endMoveRows();
        } else {
            beginInsertRows(QModelIndex(), target, target);
            m_processes.insert(target, processes.at(target));
            endInsertRows();
        }
    }

    // 2) Everything below the wanted length either exited or dropped out.
    if (m_processes.size() > processes.size()) {
        beginRemoveRows(QModelIndex(), processes.size(), m_processes.size() - 1);
        m_processes.resize(processes.size());
        endRemoveRows();
    }

    // 3) Values: report only the roles that actually changed, so one tick does
    // not repaint 758 rows worth of text that did not move.
    for (int row = 0; row < processes.size(); ++row) {
        ProcessInfo &current = m_processes[row];
        const ProcessInfo &wanted = processes.at(row);
        QList<int> roles;
        if (current.pid != wanted.pid) {
            current.pid = wanted.pid;
            roles.append(PidRole);
        }
        if (current.ppid != wanted.ppid) {
            current.ppid = wanted.ppid;
            roles.append(PpidRole);
        }
        if (current.name != wanted.name) {
            current.name = wanted.name;
            roles.append(NameRole);
        }
        if (current.command != wanted.command) {
            current.command = wanted.command;
            roles.append(CommandRole);
        }
        if (current.cpu != wanted.cpu) {
            current.cpu = wanted.cpu;
            roles.append(CpuRole);
        }
        if (current.memoryBytes != wanted.memoryBytes) {
            current.memoryBytes = wanted.memoryBytes;
            roles.append(MemoryRole);
        }
        if (current.memoryPercent != wanted.memoryPercent) {
            current.memoryPercent = wanted.memoryPercent;
            roles.append(MemoryPercentRole);
        }
        if (!roles.isEmpty()) {
            const QModelIndex changed = index(row, 0);
            Q_EMIT dataChanged(changed, changed, roles);
        }
    }
}

bool ProcessModel::removePid(int pid)
{
    for (int row = 0; row < m_processes.size(); ++row) {
        if (m_processes.at(row).pid != pid) {
            continue;
        }
        beginRemoveRows(QModelIndex(), row, row);
        m_processes.removeAt(row);
        endRemoveRows();
        return true;
    }
    return false;
}
