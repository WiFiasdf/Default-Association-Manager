#include "ui/delegates/AssociationItemDelegate.h"

#include "ui/ThemeManager.h"
#include "ui/UiKit.h"
#include "ui/models/AssociationTableModel.h"

#include <QApplication>
#include <QFontMetrics>
#include <QIcon>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>

namespace das::ui {

namespace {

constexpr int kPaddingLeft   = 14;
constexpr int kCheckBoxSize  = 18;
constexpr int kCheckColumnW  = 30;
constexpr int kIconSize      = 28;
constexpr int kIconTextGap   = 14;
constexpr int kActionWidth   = 66;
constexpr int kActionHeight  = 28;
constexpr int kActionRight   = 16;

} // namespace

AssociationItemDelegate::AssociationItemDelegate(QObject *parent)
    : QStyledItemDelegate(parent)
{
}

void AssociationItemDelegate::setSelectionMode(bool enabled)
{
    m_selectionMode = enabled;
}

void AssociationItemDelegate::setCompactMode(bool enabled)
{
    m_compactMode = enabled;
}

bool AssociationItemDelegate::updateHover(int row, const QPoint &viewportPos)
{
    const bool rowChanged = (row != m_hoveredRow);
    const bool posChanged = (viewportPos != m_hoverPos);
    m_hoveredRow = row;
    m_hoverPos   = viewportPos;
    return rowChanged || posChanged;
}

void AssociationItemDelegate::clearHover()
{
    m_hoveredRow = -1;
    m_hoverPos   = QPoint(-1, -1);
}

QSize AssociationItemDelegate::sizeHint(const QStyleOptionViewItem &option,
                                        const QModelIndex &index) const
{
    Q_UNUSED(option)
    Q_UNUSED(index)
    return {120, kRowHeight};
}

QRect AssociationItemDelegate::checkBoxRect(const QRect &cellRect) const
{
    return {cellRect.left() + kPaddingLeft, cellRect.center().y() - kCheckBoxSize / 2,
            kCheckBoxSize, kCheckBoxSize};
}

QRect AssociationItemDelegate::actionButtonRect(const QRect &cellRect) const
{
    return {cellRect.right() - kActionRight - kActionWidth,
            cellRect.center().y() - kActionHeight / 2, kActionWidth, kActionHeight};
}

// ---------------------------------------------------------------------------
// 绘制
// ---------------------------------------------------------------------------

void AssociationItemDelegate::paintRowBackground(QPainter *painter,
                                                 const QStyleOptionViewItem &option,
                                                 const QModelIndex &index) const
{
    const ThemeManager &theme = ThemeManager::instance();
    const bool checked  = index.data(AssociationTableModel::CheckedRole).toBool();
    const bool hovered  = (index.row() == m_hoveredRow);
    const bool selected = option.state.testFlag(QStyle::State_Selected);

    QColor background;
    if (checked)
        background = theme.color(QStringLiteral("accentSubtle"), QColor(0, 103, 192, 26));
    else if (selected)
        background = theme.color(QStringLiteral("controlPressed"), QColor(0, 0, 0, 16));
    else if (hovered)
        background = theme.color(QStringLiteral("controlHover"), QColor(0, 0, 0, 10));
    else if (index.row() % 2 == 1)
        background = theme.color(QStringLiteral("surfaceAlt"), QColor(0, 0, 0, 4));

    if (background.isValid()) {
        painter->setPen(Qt::NoPen);
        painter->setBrush(background);
        painter->drawRect(option.rect);
    }

    // 底部细分隔线
    painter->setPen(QPen(theme.color(QStringLiteral("borderSubtle"), QColor(0, 0, 0, 14)), 1));
    painter->drawLine(option.rect.bottomLeft(), option.rect.bottomRight());
}

void AssociationItemDelegate::paintTargetCell(QPainter *painter, const QStyleOptionViewItem &option,
                                              const QModelIndex &index) const
{
    const ThemeManager &theme = ThemeManager::instance();
    int x = option.rect.left() + kPaddingLeft;

    // 复选框
    if (m_selectionMode) {
        const QRect box = checkBoxRect(option.rect);
        const bool checked = index.data(AssociationTableModel::CheckedRole).toBool();
        const bool hovered = box.contains(m_hoverPos);

        painter->setPen(QPen(checked ? theme.accent()
                                     : theme.color(QStringLiteral("borderStrong"),
                                                   QColor(0, 0, 0, 60)),
                             hovered ? 1.6 : 1.2));
        painter->setBrush(checked ? theme.accent()
                                  : theme.color(QStringLiteral("control"), Qt::transparent));
        painter->drawRoundedRect(QRectF(box).adjusted(0.5, 0.5, -0.5, -0.5), 4, 4);

        if (checked) {
            QPainterPath path;
            path.moveTo(box.left() + box.width() * 0.24, box.top() + box.height() * 0.52);
            path.lineTo(box.left() + box.width() * 0.43, box.top() + box.height() * 0.71);
            path.lineTo(box.left() + box.width() * 0.77, box.top() + box.height() * 0.31);
            painter->setBrush(Qt::NoBrush);
            painter->setPen(QPen(theme.color(QStringLiteral("accentFg"), Qt::white), 1.9,
                                 Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            painter->drawPath(path);
        }
        x += kCheckColumnW;
    }

    // 图标
    const QVariant iconVariant = index.data(Qt::DecorationRole);
    if (iconVariant.canConvert<QIcon>()) {
        const QIcon icon = qvariant_cast<QIcon>(iconVariant);
        if (!icon.isNull()) {
            const QRect iconRect(x, option.rect.center().y() - kIconSize / 2, kIconSize, kIconSize);
            icon.paint(painter, iconRect, Qt::AlignCenter);
        }
    }
    x += kIconSize + kIconTextGap;

    const int textWidth = option.rect.right() - x - 12;
    if (textWidth <= 0)
        return;

    const QString target      = index.data(AssociationTableModel::TargetRole).toString();
    const QString description = index.data(AssociationTableModel::DescriptionRole).toString();

    QFont titleFont = scaledFont(option.font, 0.5);
    titleFont.setWeight(QFont::DemiBold);

    if (m_compactMode || description.isEmpty()) {
        painter->setFont(titleFont);
        painter->setPen(theme.text());
        painter->drawText(QRect(x, option.rect.top(), textWidth, option.rect.height()),
                          Qt::AlignVCenter | Qt::AlignLeft,
                          QFontMetrics(titleFont).elidedText(target, Qt::ElideRight, textWidth));
        return;
    }

    const QFont subFont = scaledFont(option.font, -1.2);

    const int centerY = option.rect.center().y();
    painter->setFont(titleFont);
    painter->setPen(theme.text());
    painter->drawText(QRect(x, centerY - 19, textWidth, 20), Qt::AlignVCenter | Qt::AlignLeft,
                      QFontMetrics(titleFont).elidedText(target, Qt::ElideRight, textWidth));

    painter->setFont(subFont);
    painter->setPen(theme.textTertiary());
    painter->drawText(QRect(x, centerY + 1, textWidth, 18), Qt::AlignVCenter | Qt::AlignLeft,
                      QFontMetrics(subFont).elidedText(description, Qt::ElideRight, textWidth));
}

void AssociationItemDelegate::paintAppCell(QPainter *painter, const QStyleOptionViewItem &option,
                                           const QModelIndex &index) const
{
    const ThemeManager &theme = ThemeManager::instance();

    const int x = option.rect.left() + 4;
    const int textWidth = option.rect.width() - 16;
    if (textWidth <= 0)
        return;

    QString appName = index.data(AssociationTableModel::AppNameRole).toString();
    const QString appPath = index.data(AssociationTableModel::AppPathRole).toString();
    const bool unassigned = appName.isEmpty();
    if (unassigned)
        appName = QStringLiteral("未设置");

    QFont nameFont = option.font;
    const int centerY = option.rect.center().y();

    if (m_compactMode || appPath.isEmpty()) {
        painter->setFont(nameFont);
        painter->setPen(unassigned ? theme.textTertiary() : theme.text());
        painter->drawText(QRect(x, option.rect.top(), textWidth, option.rect.height()),
                          Qt::AlignVCenter | Qt::AlignLeft,
                          QFontMetrics(nameFont).elidedText(appName, Qt::ElideRight, textWidth));
        return;
    }

    const QFont pathFont = scaledFont(option.font, -1.2);

    painter->setFont(nameFont);
    painter->setPen(unassigned ? theme.textTertiary() : theme.text());
    painter->drawText(QRect(x, centerY - 19, textWidth, 20), Qt::AlignVCenter | Qt::AlignLeft,
                      QFontMetrics(nameFont).elidedText(appName, Qt::ElideRight, textWidth));

    painter->setFont(pathFont);
    painter->setPen(theme.textTertiary());
    painter->drawText(QRect(x, centerY + 1, textWidth, 18), Qt::AlignVCenter | Qt::AlignLeft,
                      QFontMetrics(pathFont).elidedText(appPath, Qt::ElideMiddle, textWidth));
}

void AssociationItemDelegate::paintActionCell(QPainter *painter, const QStyleOptionViewItem &option,
                                              const QModelIndex &index) const
{
    const ThemeManager &theme = ThemeManager::instance();
    const QRect button = actionButtonRect(option.rect);
    const bool rowHovered    = (index.row() == m_hoveredRow);
    const bool buttonHovered = rowHovered && button.contains(m_hoverPos);

    painter->save();
    if (!rowHovered)
        painter->setOpacity(0.55);

    QColor background = theme.color(QStringLiteral("control"), QColor(0, 0, 0, 8));
    QColor border     = theme.color(QStringLiteral("border"), QColor(0, 0, 0, 26));
    QColor textColor  = theme.text();
    if (buttonHovered) {
        background = theme.accent();
        border     = theme.color(QStringLiteral("accentBorder"), theme.accent());
        textColor  = theme.color(QStringLiteral("accentFg"), Qt::white);
    }

    painter->setPen(QPen(border, 1));
    painter->setBrush(background);
    painter->drawRoundedRect(QRectF(button).adjusted(0.5, 0.5, -0.5, -0.5),
                            kActionHeight / 2.0, kActionHeight / 2.0);

    QFont font = scaledFont(option.font, -0.5, 8.0);
    font.setWeight(buttonHovered ? QFont::DemiBold : QFont::Normal);
    painter->setFont(font);
    painter->setPen(textColor);
    painter->drawText(button, Qt::AlignCenter, QStringLiteral("更改"));
    painter->restore();
}

void AssociationItemDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option,
                                    const QModelIndex &index) const
{
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setRenderHint(QPainter::TextAntialiasing, true);
    painter->setRenderHint(QPainter::SmoothPixmapTransform, true);

    paintRowBackground(painter, option, index);

    switch (index.column()) {
    case AssociationTableModel::ColumnTarget:
        paintTargetCell(painter, option, index);
        break;
    case AssociationTableModel::ColumnApp:
        paintAppCell(painter, option, index);
        break;
    case AssociationTableModel::ColumnAction:
        paintActionCell(painter, option, index);
        break;
    default:
        break;
    }

    painter->restore();
}

// ---------------------------------------------------------------------------
// 交互
// ---------------------------------------------------------------------------

bool AssociationItemDelegate::editorEvent(QEvent *event, QAbstractItemModel *model,
                                          const QStyleOptionViewItem &option,
                                          const QModelIndex &index)
{
    if (event->type() != QEvent::MouseButtonRelease)
        return QStyledItemDelegate::editorEvent(event, model, option, index);

    auto *mouseEvent = static_cast<QMouseEvent *>(event);
    if (mouseEvent->button() != Qt::LeftButton)
        return QStyledItemDelegate::editorEvent(event, model, option, index);

    const QPoint pos = mouseEvent->position().toPoint();

    if (index.column() == AssociationTableModel::ColumnAction
        && actionButtonRect(option.rect).contains(pos)) {
        emit changeRequested(index);
        return true;
    }

    if (m_selectionMode && index.column() == AssociationTableModel::ColumnTarget
        && checkBoxRect(option.rect).contains(pos)) {
        emit checkToggled(index);
        return true;
    }

    return QStyledItemDelegate::editorEvent(event, model, option, index);
}

} // namespace das::ui
