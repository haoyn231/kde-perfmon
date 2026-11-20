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

    // Incremental sync: rows that only changed values are updated in place,
    // rows that changed position are moved, unknown PIDs are inserted and PIDs
    // that disappeared are removed. The view therefore never sees a model
    // reset, so delegates survive and ListView's move / displaced transitions
    // can animate a re-sort instead of the whole table snapping into place.
    void syncProcesses(QVector<ProcessInfo> processes);

    // Read-only view of the current rows, so re-sorting does not have to round
    // trip every field through the role interface.
    const QVector<ProcessInfo> &processes() const { return m_processes; }

    // Removes a single row in place (no model reset), so the surviving rows keep
    // their order and the view keeps its scroll position.
    bool removePid(int pid);

private:
    QVector<ProcessInfo> m_processes;
};
