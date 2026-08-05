#pragma once

#include <QPoint>
#include <QStyledItemDelegate>

namespace das::ui {

/// 关联表格的自绘委托：图标 + 双行文本 + 行内胶囊按钮 + 多选复选框。
class AssociationItemDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    static constexpr int kRowHeight = 56;

    explicit AssociationItemDelegate(QObject *parent = nullptr);

    void setSelectionMode(bool enabled);
    bool selectionMode() const { return m_selectionMode; }

    void setCompactMode(bool enabled);

    /// 更新悬停状态，返回是否需要重绘。
    bool updateHover(int row, const QPoint &viewportPos);
    void clearHover();

    void  paint(QPainter *painter, const QStyleOptionViewItem &option,
                const QModelIndex &index) const override;
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;

signals:
    void changeRequested(const QModelIndex &index);
    void checkToggled(const QModelIndex &index);

protected:
    bool editorEvent(QEvent *event, QAbstractItemModel *model, const QStyleOptionViewItem &option,
                     const QModelIndex &index) override;

private:
    QRect checkBoxRect(const QRect &cellRect) const;
    QRect actionButtonRect(const QRect &cellRect) const;

    void paintTargetCell(QPainter *painter, const QStyleOptionViewItem &option,
                         const QModelIndex &index) const;
    void paintAppCell(QPainter *painter, const QStyleOptionViewItem &option,
                      const QModelIndex &index) const;
    void paintActionCell(QPainter *painter, const QStyleOptionViewItem &option,
                         const QModelIndex &index) const;
    void paintRowBackground(QPainter *painter, const QStyleOptionViewItem &option,
                            const QModelIndex &index) const;

    bool   m_selectionMode = false;
    bool   m_compactMode   = false;
    int    m_hoveredRow    = -1;
    QPoint m_hoverPos;
};

} // namespace das::ui
