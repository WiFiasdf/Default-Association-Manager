#include "ui/NavigationRail.h"

#include "ui/ThemeManager.h"
#include "ui/UiKit.h"

#include <QEvent>
#include <QFontMetrics>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPropertyAnimation>
#include <QToolTip>
#include <QVariantAnimation>

namespace das::ui {

namespace {

constexpr int kTopPadding    = 12;
constexpr int kItemHeight    = 44;
constexpr int kItemGap       = 4;
constexpr int kSideMargin    = 8;
constexpr int kIconSize      = 20;
constexpr int kIconLeftInset = 16;
constexpr int kIndicatorW    = 3;
constexpr int kIndicatorH    = 18;

} // namespace

NavigationRail::NavigationRail(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("navigationRail"));
    setAttribute(Qt::WA_Hover, true);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setFixedWidth(kExpandedWidth);

    m_topAnimation = new QPropertyAnimation(this, "indicatorTop", this);
    m_topAnimation->setDuration(180);
    m_topAnimation->setEasingCurve(QEasingCurve::OutCubic);

    m_heightAnimation = new QPropertyAnimation(this, "indicatorHeight", this);
    m_heightAnimation->setDuration(200);
    m_heightAnimation->setEasingCurve(QEasingCurve::OutBack);

    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this,
            [this](bool) { update(); });
}

void NavigationRail::addItem(const QString &iconPath, const QString &title,
                             const QString &description)
{
    m_items.append(Item{iconPath, title, description});
    if (m_current < 0)
        setCurrentIndex(0);
    updateGeometry();
    update();
}

QSize NavigationRail::sizeHint() const
{
    const int height = kTopPadding * 2 + m_items.size() * kItemHeight
                       + qMax(0, m_items.size() - 1) * kItemGap;
    return {m_collapsed ? kCollapsedWidth : kExpandedWidth, height};
}

QSize NavigationRail::minimumSizeHint() const
{
    return {kCollapsedWidth, kTopPadding * 2 + kItemHeight};
}

QRect NavigationRail::itemRect(int index) const
{
    if (index < 0 || index >= m_items.size())
        return {};
    const int y = kTopPadding + index * (kItemHeight + kItemGap);
    return {kSideMargin, y, qMax(0, width() - kSideMargin * 2), kItemHeight};
}

int NavigationRail::indexAt(const QPoint &pos) const
{
    for (int i = 0; i < m_items.size(); ++i) {
        if (itemRect(i).contains(pos))
            return i;
    }
    return -1;
}

void NavigationRail::setIndicatorTop(qreal value)
{
    if (qFuzzyCompare(m_indicatorTop, value))
        return;
    m_indicatorTop = value;
    update();
}

void NavigationRail::setIndicatorHeight(qreal value)
{
    if (qFuzzyCompare(m_indicatorHeight, value))
        return;
    m_indicatorHeight = qMax(0.0, value);
    update();
}

void NavigationRail::moveIndicatorTo(int index, bool animated)
{
    const QRect rect = itemRect(index);
    if (!rect.isValid())
        return;

    const qreal targetTop    = rect.center().y() - kIndicatorH / 2.0 + 1.0;
    const qreal targetHeight = kIndicatorH;

    m_topAnimation->stop();
    m_heightAnimation->stop();

    if (!animated || !isVisible()) {
        setIndicatorTop(targetTop);
        setIndicatorHeight(targetHeight);
        return;
    }

    m_topAnimation->setStartValue(m_indicatorTop);
    m_topAnimation->setEndValue(targetTop);
    m_topAnimation->start();

    // 高度先压缩再回弹，制造轻微的弹性
    m_heightAnimation->setStartValue(qMax(6.0, m_indicatorHeight * 0.45));
    m_heightAnimation->setEndValue(targetHeight);
    m_heightAnimation->start();
}

void NavigationRail::setCurrentIndex(int index)
{
    if (index < 0 || index >= m_items.size() || index == m_current)
        return;
    const bool first = (m_current < 0);
    m_current = index;
    moveIndicatorTo(index, !first);
    update();
    emit currentChanged(index);
}

void NavigationRail::setCollapsed(bool collapsed)
{
    if (m_collapsed == collapsed)
        return;
    m_collapsed = collapsed;

    const int target = collapsed ? kCollapsedWidth : kExpandedWidth;
    if (!m_widthAnimation) {
        m_widthAnimation = new QVariantAnimation(this);
        m_widthAnimation->setDuration(200);
        m_widthAnimation->setEasingCurve(QEasingCurve::OutCubic);
        connect(m_widthAnimation, &QVariantAnimation::valueChanged, this,
                [this](const QVariant &value) { setFixedWidth(value.toInt()); });
    }
    m_widthAnimation->stop();
    m_widthAnimation->setStartValue(width());
    m_widthAnimation->setEndValue(target);
    m_widthAnimation->start();
    update();
}

void NavigationRail::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    const ThemeManager &theme = ThemeManager::instance();
    const QColor hoverColor    = theme.color(QStringLiteral("navHover"), QColor(0, 0, 0, 12));
    const QColor selectedColor = theme.color(QStringLiteral("navSelected"), QColor(0, 0, 0, 22));
    const QColor accentColor   = theme.accent();
    const QColor textColor     = theme.text();
    const QColor mutedColor    = theme.textSecondary();

    for (int i = 0; i < m_items.size(); ++i) {
        const QRect rect = itemRect(i);
        if (!rect.isValid())
            continue;

        const bool selected = (i == m_current);
        const bool hovered  = (i == m_hover);

        if (selected || hovered) {
            QRect fill = rect;
            if (m_pressed == i)
                fill.adjust(1, 1, -1, -1);
            painter.setPen(Qt::NoPen);
            painter.setBrush(selected ? selectedColor : hoverColor);
            painter.drawRoundedRect(fill, 6, 6);
        }

        // 图标
        const QColor iconColor = selected ? accentColor : mutedColor;
        const QPixmap icon = renderSvg(m_items.at(i).iconPath, kIconSize, iconColor,
                                       devicePixelRatioF());
        const int iconX = m_collapsed ? rect.center().x() - kIconSize / 2
                                      : rect.left() + kIconLeftInset;
        const int iconY = rect.center().y() - kIconSize / 2;
        if (!icon.isNull())
            painter.drawPixmap(QRect(iconX, iconY, kIconSize, kIconSize), icon);

        // 文本
        if (!m_collapsed) {
            const int textLeft = rect.left() + kIconLeftInset + kIconSize + 14;
            const QRect textRect(textLeft, rect.top(), rect.right() - textLeft - 10, rect.height());
            QFont font = this->font();
            font.setWeight(selected ? QFont::DemiBold : QFont::Normal);
            painter.setFont(font);
            painter.setPen(selected ? textColor : mutedColor);
            const QString elided =
                QFontMetrics(font).elidedText(m_items.at(i).title, Qt::ElideRight, textRect.width());
            painter.drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft, elided);
        }
    }

    // 选中指示条
    if (m_current >= 0 && m_indicatorHeight > 0.5) {
        const QRect rect = itemRect(m_current);
        QRectF bar(rect.left() + 3.0, m_indicatorTop, qreal(kIndicatorW), m_indicatorHeight);
        painter.setPen(Qt::NoPen);
        painter.setBrush(accentColor);
        painter.drawRoundedRect(bar, kIndicatorW / 2.0, kIndicatorW / 2.0);
    }
}

void NavigationRail::mouseMoveEvent(QMouseEvent *event)
{
    const int index = indexAt(event->pos());
    if (index != m_hover) {
        m_hover = index;
        setCursor(index >= 0 ? Qt::PointingHandCursor : Qt::ArrowCursor);
        update();
    }
    QWidget::mouseMoveEvent(event);
}

void NavigationRail::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) {
        QWidget::mousePressEvent(event);
        return;
    }
    const int index = indexAt(event->pos());
    m_pressed = index;
    update();
    if (index >= 0)
        setCurrentIndex(index);
    m_pressed = -1;
}

void NavigationRail::leaveEvent(QEvent *event)
{
    if (m_hover != -1) {
        m_hover = -1;
        update();
    }
    QWidget::leaveEvent(event);
}

void NavigationRail::keyPressEvent(QKeyEvent *event)
{
    switch (event->key()) {
    case Qt::Key_Down:
        setCurrentIndex(qMin(m_current + 1, m_items.size() - 1));
        return;
    case Qt::Key_Up:
        setCurrentIndex(qMax(m_current - 1, 0));
        return;
    default:
        break;
    }
    QWidget::keyPressEvent(event);
}

bool NavigationRail::event(QEvent *event)
{
    if (event->type() == QEvent::ToolTip) {
        auto *helpEvent = static_cast<QHelpEvent *>(event);
        const int index = indexAt(helpEvent->pos());
        if (index >= 0) {
            const Item &item = m_items.at(index);
            const QString tip = m_collapsed || item.description.isEmpty()
                                    ? (item.description.isEmpty()
                                           ? item.title
                                           : item.title + QLatin1String(" · ") + item.description)
                                    : item.description;
            QToolTip::showText(helpEvent->globalPos(), tip, this);
        } else {
            QToolTip::hideText();
        }
        return true;
    }
    return QWidget::event(event);
}

} // namespace das::ui
