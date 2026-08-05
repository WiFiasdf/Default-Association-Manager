#include "ui/models/AssociationTableModel.h"

#include "core/Category.h"
#include "platform/win/IconProvider.h"

#include <QIcon>

namespace das::ui {

AssociationTableModel::AssociationTableModel(QObject *parent)
    : QAbstractTableModel(parent)
{
    connect(&win::IconProvider::instance(), &win::IconProvider::iconReady, this,
            &AssociationTableModel::onIconReady);
}

// ---------------------------------------------------------------------------
// 基本接口
// ---------------------------------------------------------------------------

int AssociationTableModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_entries.size();
}

int AssociationTableModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : ColumnCount;
}

QString AssociationTableModel::indexKey(const QString &target, bool isProtocol) const
{
    return (isProtocol ? QStringLiteral("p:") : QStringLiteral("f:")) + target.toLower();
}

QString AssociationTableModel::iconKeyFor(const AssociationEntry &entry) const
{
    // 文件类型用扩展名取系统文档图标；协议没有扩展名，退回到程序 exe 图标
    if (!entry.isProtocol)
        return entry.target;
    return entry.appPath;
}

QVariant AssociationTableModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_entries.size())
        return {};

    const AssociationEntry &entry = m_entries.at(index.row());

    switch (role) {
    case Qt::DisplayRole:
        switch (index.column()) {
        case ColumnTarget:
            return entry.target;
        case ColumnApp:
            return entry.appName.isEmpty() ? QStringLiteral("未设置") : entry.appName;
        default:
            return {};
        }
    case Qt::DecorationRole:
        if (index.column() == ColumnTarget) {
            const QString key = iconKeyFor(entry);
            if (key.isEmpty())
                return win::IconProvider::instance().placeholder();
            const_cast<AssociationTableModel *>(this)->registerIconKey(key, index.row());
            return win::IconProvider::instance().iconFor(key);
        }
        return {};
    case Qt::ToolTipRole:
        if (index.column() == ColumnApp && !entry.appPath.isEmpty())
            return entry.appPath;
        if (index.column() == ColumnTarget)
            return entry.typeDescription;
        return {};
    case EntryRole:
        return QVariant::fromValue(entry);
    case TargetRole:
        return entry.target;
    case IsProtocolRole:
        return entry.isProtocol;
    case CategoryRole:
        return static_cast<int>(entry.category);
    case ProgIdRole:
        return entry.progId;
    case AppNameRole:
        return entry.appName;
    case AppPathRole:
        return entry.appPath;
    case DescriptionRole:
        return entry.typeDescription.isEmpty() ? fallbackTypeDescription(entry.target)
                                               : entry.typeDescription;
    case CheckedRole:
        return m_checked.contains(index.row());
    case IconKeyRole:
        return iconKeyFor(entry);
    default:
        break;
    }
    return {};
}

QVariant AssociationTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return QAbstractTableModel::headerData(section, orientation, role);

    switch (section) {
    case ColumnTarget:
        return QStringLiteral("文件类型");
    case ColumnApp:
        return QStringLiteral("当前默认程序");
    case ColumnAction:
        return QStringLiteral("操作");
    default:
        break;
    }
    return {};
}

Qt::ItemFlags AssociationTableModel::flags(const QModelIndex &index) const
{
    if (!index.isValid())
        return Qt::NoItemFlags;
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

// ---------------------------------------------------------------------------
// 数据维护
// ---------------------------------------------------------------------------

void AssociationTableModel::clear()
{
    beginResetModel();
    m_entries.clear();
    m_rowByTarget.clear();
    m_rowsByIconKey.clear();
    const bool hadChecked = !m_checked.isEmpty();
    m_checked.clear();
    endResetModel();
    if (hadChecked)
        emit checkedCountChanged(0);
}

void AssociationTableModel::appendEntries(const AssociationEntryList &batch)
{
    if (batch.isEmpty())
        return;

    const int first = m_entries.size();
    beginInsertRows(QModelIndex(), first, first + batch.size() - 1);
    m_entries.append(batch);
    for (int i = 0; i < batch.size(); ++i) {
        const AssociationEntry &entry = batch.at(i);
        m_rowByTarget.insert(indexKey(entry.target, entry.isProtocol), first + i);
    }
    endInsertRows();
}

void AssociationTableModel::updateEntry(const AssociationEntry &entry)
{
    const int row = rowOf(entry.target, entry.isProtocol);
    if (row < 0)
        return;
    m_entries[row] = entry;
    emit dataChanged(index(row, 0), index(row, ColumnCount - 1));
}

AssociationEntry AssociationTableModel::entryAt(int row) const
{
    if (row < 0 || row >= m_entries.size())
        return {};
    return m_entries.at(row);
}

int AssociationTableModel::rowOf(const QString &target, bool isProtocol) const
{
    return m_rowByTarget.value(indexKey(target, isProtocol), -1);
}

void AssociationTableModel::registerIconKey(const QString &iconKey, int row)
{
    QList<int> &rows = m_rowsByIconKey[iconKey];
    if (!rows.contains(row))
        rows.append(row);
}

void AssociationTableModel::onIconReady(const QString &key)
{
    const auto it = m_rowsByIconKey.constFind(key);
    if (it == m_rowsByIconKey.constEnd())
        return;
    for (int row : it.value()) {
        if (row >= 0 && row < m_entries.size()) {
            const QModelIndex idx = index(row, ColumnTarget);
            emit dataChanged(idx, idx, {Qt::DecorationRole});
        }
    }
}

// ---------------------------------------------------------------------------
// 勾选
// ---------------------------------------------------------------------------

void AssociationTableModel::setChecked(int row, bool checked)
{
    if (row < 0 || row >= m_entries.size())
        return;
    const bool has = m_checked.contains(row);
    if (has == checked)
        return;
    if (checked)
        m_checked.insert(row);
    else
        m_checked.remove(row);

    const QModelIndex idx = index(row, ColumnTarget);
    emit dataChanged(idx, index(row, ColumnCount - 1), {CheckedRole});
    emit checkedCountChanged(m_checked.size());
}

void AssociationTableModel::toggleChecked(int row)
{
    setChecked(row, !m_checked.contains(row));
}

void AssociationTableModel::setCheckedRows(const QList<int> &rows, bool checked)
{
    if (rows.isEmpty())
        return;
    bool changed = false;
    for (int row : rows) {
        if (row < 0 || row >= m_entries.size())
            continue;
        if (checked) {
            if (!m_checked.contains(row)) {
                m_checked.insert(row);
                changed = true;
            }
        } else if (m_checked.remove(row)) {
            changed = true;
        }
    }
    if (!changed)
        return;

    emit dataChanged(index(0, 0), index(m_entries.size() - 1, ColumnCount - 1), {CheckedRole});
    emit checkedCountChanged(m_checked.size());
}

void AssociationTableModel::clearChecked()
{
    if (m_checked.isEmpty())
        return;
    m_checked.clear();
    if (!m_entries.isEmpty())
        emit dataChanged(index(0, 0), index(m_entries.size() - 1, ColumnCount - 1), {CheckedRole});
    emit checkedCountChanged(0);
}

AssociationEntryList AssociationTableModel::checkedEntries() const
{
    AssociationEntryList list;
    list.reserve(m_checked.size());
    for (int row : m_checked) {
        if (row >= 0 && row < m_entries.size())
            list.append(m_entries.at(row));
    }
    return list;
}

} // namespace das::ui
