#pragma once

#include "core/Types.h"

#include <QSortFilterProxyModel>
#include <QString>

namespace das::ui {

/// 关键词 + 类别复合过滤。关键词同时匹配扩展名、类型说明与程序名。
class AssociationFilterProxy : public QSortFilterProxyModel
{
    Q_OBJECT

public:
    explicit AssociationFilterProxy(QObject *parent = nullptr);

    void setKeyword(const QString &keyword);
    QString keyword() const { return m_keyword; }

    void setCategory(FileCategory category);
    FileCategory category() const { return m_category; }

    /// 只显示「未设置默认程序」的项。
    void setOnlyUnassigned(bool only);

    /// 当前可见的所有源模型行号。
    QList<int> visibleSourceRows() const;

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;
    bool lessThan(const QModelIndex &left, const QModelIndex &right) const override;

private:
    QString      m_keyword;
    FileCategory m_category        = FileCategory::All;
    bool         m_onlyUnassigned  = false;
};

} // namespace das::ui
