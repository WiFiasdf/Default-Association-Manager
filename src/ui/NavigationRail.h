#pragma once

#include <QString>
#include <QVector>
#include <QWidget>

class QPropertyAnimation;
class QVariantAnimation;

namespace das::ui {

/// 左侧垂直导航栏。自绘实现，支持选中指示条弹性动画与折叠态。
class NavigationRail : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(qreal indicatorTop READ indicatorTop WRITE setIndicatorTop)
    Q_PROPERTY(qreal indicatorHeight READ indicatorHeight WRITE setIndicatorHeight)

public:
    static constexpr int kExpandedWidth  = 220;
    static constexpr int kCollapsedWidth = 60;

    explicit NavigationRail(QWidget *parent = nullptr);

    void addItem(const QString &iconPath, const QString &title,
                 const QString &description = QString());

    int  count() const { return m_items.size(); }
    int  currentIndex() const { return m_current; }

    void setCollapsed(bool collapsed);
    bool isCollapsed() const { return m_collapsed; }

    qreal indicatorTop() const { return m_indicatorTop; }
    void  setIndicatorTop(qreal value);
    qreal indicatorHeight() const { return m_indicatorHeight; }
    void  setIndicatorHeight(qreal value);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

public slots:
    void setCurrentIndex(int index);

signals:
    void currentChanged(int index);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    bool event(QEvent *event) override;

private:
    struct Item {
        QString iconPath;
        QString title;
        QString description;
    };

    QRect itemRect(int index) const;
    int   indexAt(const QPoint &pos) const;
    void  moveIndicatorTo(int index, bool animated);

    QVector<Item> m_items;
    int           m_current   = -1;
    int           m_hover     = -1;
    int           m_pressed   = -1;
    bool          m_collapsed = false;

    qreal m_indicatorTop    = 0.0;
    qreal m_indicatorHeight = 0.0;

    QPropertyAnimation *m_topAnimation    = nullptr;
    QPropertyAnimation *m_heightAnimation = nullptr;
    QVariantAnimation  *m_widthAnimation  = nullptr;
};

} // namespace das::ui
