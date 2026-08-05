#pragma once

#include "core/Types.h"

#include <QAbstractTableModel>
#include <QHash>
#include <QList>
#include <QSet>
#include <QString>

namespace das::ui {

/// 关联表格模型。
///
/// 性能设计：
///   - 扫描结果以 200 条为一批 append，避免逐行 beginInsertRows 的开销；
///   - 图标只在 Qt::DecorationRole 被真正请求时异步加载（QTableView 仅对可视行
///     调用 data()），因此天然实现「按需加载」；
///   - 图标就绪后只刷新对应行，不做全表 reset。
class AssociationTableModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    enum Column {
        ColumnTarget = 0, ///< 扩展名 / 协议 + 类型说明
        ColumnApp,        ///< 当前默认程序 + 路径
        ColumnAction,     ///< 行内操作
        ColumnCount
    };

    enum Roles {
        EntryRole = Qt::UserRole + 1,
        TargetRole,
        IsProtocolRole,
        CategoryRole,
        ProgIdRole,
        AppNameRole,
        AppPathRole,
        DescriptionRole,
        CheckedRole,
        IconKeyRole
    };

    explicit AssociationTableModel(QObject *parent = nullptr);

    // --- QAbstractTableModel ------------------------------------------------
    int      rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int      columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;

    // --- 数据维护 -----------------------------------------------------------
    void clear();
    void appendEntries(const AssociationEntryList &batch);
    void updateEntry(const AssociationEntry &entry);

    AssociationEntry entryAt(int row) const;
    int              rowOf(const QString &target, bool isProtocol) const;

    // --- 勾选 ---------------------------------------------------------------
    void setChecked(int row, bool checked);
    void toggleChecked(int row);
    void setCheckedRows(const QList<int> &rows, bool checked);
    void clearChecked();

    int                  checkedCount() const { return m_checked.size(); }
    AssociationEntryList checkedEntries() const;

signals:
    void checkedCountChanged(int count);

private slots:
    void onIconReady(const QString &key);

private:
    QString iconKeyFor(const AssociationEntry &entry) const;
    QString indexKey(const QString &target, bool isProtocol) const;
    void    registerIconKey(const QString &iconKey, int row);

    AssociationEntryList              m_entries;
    QHash<QString, int>               m_rowByTarget;  ///< "f:.png" -> row
    QHash<QString, QList<int>>        m_rowsByIconKey;
    QSet<int>                         m_checked;
};

} // namespace das::ui
