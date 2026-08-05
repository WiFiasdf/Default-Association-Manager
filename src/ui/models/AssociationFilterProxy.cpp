#include "ui/models/AssociationFilterProxy.h"

#include "ui/models/AssociationTableModel.h"

namespace das::ui {

AssociationFilterProxy::AssociationFilterProxy(QObject *parent)
    : QSortFilterProxyModel(parent)
{
    setDynamicSortFilter(true);
    setSortCaseSensitivity(Qt::CaseInsensitive);
}

void AssociationFilterProxy::setKeyword(const QString &keyword)
{
    const QString trimmed = keyword.trimmed();
    if (m_keyword == trimmed)
        return;
    m_keyword = trimmed;
    invalidateRowsFilter();
}

void AssociationFilterProxy::setCategory(FileCategory category)
{
    if (m_category == category)
        return;
    m_category = category;
    invalidateRowsFilter();
}

void AssociationFilterProxy::setOnlyUnassigned(bool only)
{
    if (m_onlyUnassigned == only)
        return;
    m_onlyUnassigned = only;
    invalidateRowsFilter();
}

bool AssociationFilterProxy::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
    const QAbstractItemModel *model = sourceModel();
    if (!model)
        return false;

    const QModelIndex idx = model->index(sourceRow, AssociationTableModel::ColumnTarget,
                                         sourceParent);
    if (!idx.isValid())
        return false;

    if (m_category != FileCategory::All) {
        const auto category =
            static_cast<FileCategory>(idx.data(AssociationTableModel::CategoryRole).toInt());
        if (category != m_category)
            return false;
    }

    if (m_onlyUnassigned && !idx.data(AssociationTableModel::ProgIdRole).toString().isEmpty())
        return false;

    if (m_keyword.isEmpty())
        return true;

    const QString target = idx.data(AssociationTableModel::TargetRole).toString();
    if (target.contains(m_keyword, Qt::CaseInsensitive))
        return true;

    const QString appName = idx.data(AssociationTableModel::AppNameRole).toString();
    if (appName.contains(m_keyword, Qt::CaseInsensitive))
        return true;

    const QString description = idx.data(AssociationTableModel::DescriptionRole).toString();
    if (description.contains(m_keyword, Qt::CaseInsensitive))
        return true;

    return false;
}

bool AssociationFilterProxy::lessThan(const QModelIndex &left, const QModelIndex &right) const
{
    const int column = left.column();
    if (column == AssociationTableModel::ColumnApp) {
        const QString a = left.data(AssociationTableModel::AppNameRole).toString();
        const QString b = right.data(AssociationTableModel::AppNameRole).toString();
        // 未设置的排到最后
        if (a.isEmpty() != b.isEmpty())
            return b.isEmpty();
        const int cmp = a.compare(b, Qt::CaseInsensitive);
        if (cmp != 0)
            return cmp < 0;
    }

    const QString a = left.data(AssociationTableModel::TargetRole).toString();
    const QString b = right.data(AssociationTableModel::TargetRole).toString();
    return a.compare(b, Qt::CaseInsensitive) < 0;
}

QList<int> AssociationFilterProxy::visibleSourceRows() const
{
    QList<int> rows;
    const int count = rowCount();
    rows.reserve(count);
    for (int i = 0; i < count; ++i)
        rows.append(mapToSource(index(i, 0)).row());
    return rows;
}

} // namespace das::ui
