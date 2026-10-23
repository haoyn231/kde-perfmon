#pragma once

#include "procdata.h"

#include <QAbstractListModel>

class ProcessModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Role {
        PidRole = Qt::UserRole + 1,
        PpidRole,
        NameRole,
        CommandRole,
        CpuRole,
        MemoryRole,
        MemoryPercentRole,
    };

    explicit ProcessModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setProcesses(QVector<ProcessInfo> processes);

private:
    QVector<ProcessInfo> m_processes;
};
