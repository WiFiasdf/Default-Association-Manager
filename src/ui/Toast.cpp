#include "ui/Toast.h"

#include "ui/ThemeManager.h"
#include "ui/UiKit.h"

#include <QEvent>
#include <QGraphicsOpacityEffect>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QParallelAnimationGroup>
#include <QPropertyAnimation>
#include <QTimer>

namespace das::ui {

namespace {

constexpr int kMarginRight  = 24;
constexpr int kMarginBottom = 24;
constexpr int kGap          = 10;
constexpr int kMaxVisible   = 4;
constexpr int kMaxWidth     = 420;

QColor levelColor(Toast::Level level)
{
    const ThemeManager &theme = ThemeManager::instance();
    switch (level) {
    case Toast::Success:
        return theme.color(QStringLiteral("success"), QColor(0x0F, 0x7B, 0x0F));
    case Toast::Warning:
        return theme.color(QStringLiteral("warning"), QColor(0x9D, 0x5D, 0x00));
    case Toast::Error:
        return theme.color(QStringLiteral("danger"), QColor(0xC4, 0x2B, 0x1C));
    case Toast::Info:
        break;
    }
    return theme.accent();
}

} // namespace

Toast::Toast(QWidget *host, const QString &text, Level level)
    : QFrame(host)
{
    setObjectName(QStringLiteral("toast"));
    setAttribute(Qt::WA_StyledBackground, true);
    setAttribute(Qt::WA_TransparentForMouseEvents, false);
    setCursor(Qt::PointingHandCursor);

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(12, 10, 16, 10);
    layout->setSpacing(11);

    m_bar = new QLabel(this);
    m_bar->setFixedWidth(4);
    m_bar->setMinimumHeight(18);
    m_bar->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    m_bar->setStyleSheet(QStringLiteral("background:%1;border-radius:2px;")
                             .arg(levelColor(level).name(QColor::HexRgb)));
    layout->addWidget(m_bar);

    m_text = new QLabel(text, this);
    m_text->setObjectName(QStringLiteral("toastText"));
    m_text->setWordWrap(true);
    m_text->setMaximumWidth(kMaxWidth);
    layout->addWidget(m_text, 1);

    adjustSize();
    setMaximumWidth(kMaxWidth + 40);
}

void Toast::showMessage(QWidget *anchor, const QString &text, Level level, int durationMs)
{
    if (!anchor || text.isEmpty())
        return;
    QWidget *host = anchor->window();
    if (!host)
        return;

    // 超出上限时先请走最早的一条
    const QList<Toast *> existing = host->findChildren<Toast *>(QString(), Qt::FindDirectChildrenOnly);
    int alive = 0;
    for (Toast *toast : existing) {
        if (!toast->m_dismissing)
            ++alive;
    }
    for (Toast *toast : existing) {
        if (alive <= kMaxVisible - 1)
            break;
        if (!toast->m_dismissing) {
            toast->dismiss();
            --alive;
        }
    }

    auto *toast = new Toast(host, text, level);
    toast->appear(durationMs);
}

void Toast::appear(int durationMs)
{
    QWidget *host = parentWidget();
    if (!host) {
        deleteLater();
        return;
    }

    adjustSize();
    const int startX = host->width() - width() - kMarginRight;
    move(startX, host->height());
    QWidget::show();
    raise();

    auto *effect = new QGraphicsOpacityEffect(this);
    effect->setOpacity(0.0);
    setGraphicsEffect(effect);

    auto *fade = new QPropertyAnimation(effect, "opacity", this);
    fade->setDuration(200);
    fade->setStartValue(0.0);
    fade->setEndValue(1.0);
    fade->setEasingCurve(QEasingCurve::OutCubic);
    fade->start(QAbstractAnimation::DeleteWhenStopped);

    relayout(host);

    QTimer::singleShot(qMax(1200, durationMs), this, [this]() { dismiss(); });
}

void Toast::dismiss()
{
    if (m_dismissing)
        return;
    m_dismissing = true;

    QWidget *host = parentWidget();

    auto *effect = qobject_cast<QGraphicsOpacityEffect *>(graphicsEffect());
    if (!effect) {
        effect = new QGraphicsOpacityEffect(this);
        setGraphicsEffect(effect);
    }

    auto *group = new QParallelAnimationGroup(this);

    auto *fade = new QPropertyAnimation(effect, "opacity");
    fade->setDuration(180);
    fade->setStartValue(effect->opacity());
    fade->setEndValue(0.0);
    group->addAnimation(fade);

    auto *slide = new QPropertyAnimation(this, "pos");
    slide->setDuration(180);
    slide->setStartValue(pos());
    slide->setEndValue(pos() + QPoint(0, 10));
    slide->setEasingCurve(QEasingCurve::InCubic);
    group->addAnimation(slide);

    connect(group, &QAbstractAnimation::finished, this, [this, host]() {
        hide();
        deleteLater();
        if (host)
            relayout(host);
    });
    group->start(QAbstractAnimation::DeleteWhenStopped);

    if (host)
        relayout(host);
}

void Toast::relayout(QWidget *host)
{
    if (!host)
        return;
    const QList<Toast *> toasts = host->findChildren<Toast *>(QString(), Qt::FindDirectChildrenOnly);

    int y = host->height() - kMarginBottom;
    // 最新的在最下方，向上堆叠
    for (auto it = toasts.crbegin(); it != toasts.crend(); ++it) {
        Toast *toast = *it;
        if (toast->m_dismissing)
            continue;
        toast->adjustSize();
        y -= toast->height();
        const QPoint target(host->width() - toast->width() - kMarginRight, y);
        y -= kGap;

        if (toast->pos() == target)
            continue;
        auto *move = new QPropertyAnimation(toast, "pos", toast);
        move->setDuration(220);
        move->setEasingCurve(QEasingCurve::OutCubic);
        move->setStartValue(toast->pos());
        move->setEndValue(target);
        move->start(QAbstractAnimation::DeleteWhenStopped);
        toast->raise();
    }
}

void Toast::mousePressEvent(QMouseEvent *event)
{
    Q_UNUSED(event)
    dismiss();
}

} // namespace das::ui
