#include "ui/UiKit.h"

#include "ui/ThemeManager.h"

#include <QApplication>
#include <QBuffer>
#include <QFile>
#include <QGraphicsDropShadowEffect>
#include <QHash>
#include <QImage>
#include <QImageReader>
#include <QLabel>
#include <QPainter>
#include <QStyle>
#include <QVariant>

#ifdef DAS_HAVE_QTSVG
#  include <QSvgRenderer>
#endif

namespace das::ui {

namespace {

QHash<QString, QPixmap> &iconCache()
{
    static QHash<QString, QPixmap> cache;
    return cache;
}

QString cacheKey(const QString &path, int size, const QColor &color, qreal dpr)
{
    return path + QLatin1Char('|') + QString::number(size) + QLatin1Char('|')
           + color.name(QColor::HexArgb) + QLatin1Char('|') + QString::number(dpr, 'f', 2);
}

} // namespace

QPixmap renderSvg(const QString &resourcePath, int size, const QColor &color, qreal dpr)
{
    if (size <= 0)
        return {};
    if (dpr <= 0.0)
        dpr = qApp ? qApp->devicePixelRatio() : 1.0;

    const QString key = cacheKey(resourcePath, size, color, dpr);
    auto &cache = iconCache();
    const auto cached = cache.constFind(key);
    if (cached != cache.constEnd())
        return cached.value();

    QFile file(resourcePath);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    QByteArray data = file.readAll();
    file.close();

    if (color.isValid())
        data.replace("currentColor", color.name(QColor::HexRgb).toUtf8());

    const QSize physical(qMax(1, qRound(size * dpr)), qMax(1, qRound(size * dpr)));
    QImage image(physical, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);

#ifdef DAS_HAVE_QTSVG
    QSvgRenderer renderer(data);
    if (renderer.isValid()) {
        QPainter painter(&image);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
        renderer.render(&painter, QRectF(QPointF(0, 0), QSizeF(physical)));
    }
#else
    QBuffer buffer(&data);
    buffer.open(QIODevice::ReadOnly);
    QImageReader reader(&buffer, QByteArrayLiteral("svg"));
    reader.setScaledSize(physical);
    const QImage decoded = reader.read();
    if (!decoded.isNull())
        image = decoded.convertToFormat(QImage::Format_ARGB32_Premultiplied);
#endif

    QPixmap pixmap = QPixmap::fromImage(image);
    pixmap.setDevicePixelRatio(dpr);

    if (cache.size() > 512)
        cache.clear();
    cache.insert(key, pixmap);
    return pixmap;
}

QIcon themedIcon(const QString &resourcePath, const QColor &color, int size)
{
    QIcon icon;
    const QPixmap normal = renderSvg(resourcePath, size, color);
    if (normal.isNull())
        return icon;
    icon.addPixmap(normal, QIcon::Normal);

    QColor disabled = color;
    disabled.setAlphaF(0.38f);
    const QPixmap disabledPixmap = renderSvg(resourcePath, size, disabled);
    if (!disabledPixmap.isNull())
        icon.addPixmap(disabledPixmap, QIcon::Disabled);
    return icon;
}

void clearIconCache()
{
    iconCache().clear();
}

void repolish(QWidget *widget)
{
    if (!widget || !widget->style())
        return;
    widget->style()->unpolish(widget);
    widget->style()->polish(widget);
    widget->update();
}

void setDynamicProperty(QWidget *widget, const char *name, const QVariant &value)
{
    if (!widget)
        return;
    widget->setProperty(name, value);
    repolish(widget);
}

void applyShadow(QWidget *widget, int blurRadius, int alpha, int dy)
{
    if (!widget)
        return;
    if (alpha <= 0) {
        widget->setGraphicsEffect(nullptr);
        return;
    }
    auto *effect = new QGraphicsDropShadowEffect(widget);
    effect->setBlurRadius(blurRadius);
    effect->setOffset(0, dy);
    effect->setColor(QColor(0, 0, 0, alpha));
    widget->setGraphicsEffect(effect);
}

QLabel *makeLabel(const QString &text, const char *role, QWidget *parent)
{
    auto *label = new QLabel(text, parent);
    if (role)
        label->setProperty("role", QString::fromLatin1(role));
    label->setTextInteractionFlags(Qt::TextSelectableByMouse);
    return label;
}

QLabel *makePageTitle(const QString &text, QWidget *parent)
{
    return makeLabel(text, "pageTitle", parent);
}

QLabel *makePageSubtitle(const QString &text, QWidget *parent)
{
    auto *label = makeLabel(text, "pageSubtitle", parent);
    label->setWordWrap(true);
    return label;
}

QLabel *makeSectionTitle(const QString &text, QWidget *parent)
{
    return makeLabel(text, "sectionTitle", parent);
}

QLabel *makeCaption(const QString &text, QWidget *parent)
{
    auto *label = makeLabel(text, "caption", parent);
    label->setWordWrap(true);
    return label;
}

QFrame *makeSeparator(Qt::Orientation orientation, QWidget *parent)
{
    auto *line = new QFrame(parent);
    line->setProperty("role", QStringLiteral("separator"));
    if (orientation == Qt::Horizontal)
        line->setFixedHeight(1);
    else
        line->setFixedWidth(1);
    return line;
}

QFont scaledFont(const QFont &base, qreal deltaPt, qreal minPt)
{
    QFont font = base;
    const qreal pt = base.pointSizeF();
    if (pt > 0.0) {
        font.setPointSizeF(qMax(minPt, pt + deltaPt));
        return font;
    }

    const int px = base.pixelSize();
    if (px > 0) {
        // 96 DPI 下 1pt ≈ 4/3 px
        const int minPx = qMax(1, qRound(minPt * 4.0 / 3.0));
        font.setPixelSize(qMax(minPx, qRound(px + deltaPt * 4.0 / 3.0)));
        return font;
    }

    // 极少数情况下两者都无效，退化为应用默认字号
    font.setPointSizeF(qMax(minPt, 10.0 + deltaPt));
    return font;
}

// ---------------------------------------------------------------------------
// Card
// ---------------------------------------------------------------------------

Card::Card(QWidget *parent, bool flat, bool shadow)
    : QFrame(parent)
{
    setObjectName(flat ? QStringLiteral("cardFlat") : QStringLiteral("card"));
    setAttribute(Qt::WA_StyledBackground, true);
    if (shadow && !flat)
        applyShadow(this, 22, 26, 3);
}

// ---------------------------------------------------------------------------
// PillButton
// ---------------------------------------------------------------------------

namespace {

const char *variantName(PillButton::Variant variant)
{
    switch (variant) {
    case PillButton::Primary:
        return "primary";
    case PillButton::Subtle:
        return "subtle";
    case PillButton::Danger:
        return "danger";
    case PillButton::Secondary:
        break;
    }
    return "secondary";
}

} // namespace

PillButton::PillButton(const QString &text, Variant variant, QWidget *parent)
    : QPushButton(text, parent)
{
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::StrongFocus);
    setVariant(variant);

    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this,
            [this](bool) { refreshIcon(); });
}

void PillButton::setVariant(Variant variant)
{
    m_variant = variant;
    setProperty("variant", QString::fromLatin1(variantName(variant)));
    repolish(this);
    refreshIcon();
}

void PillButton::setPill(bool pill)
{
    setDynamicProperty(this, "pill", pill ? QStringLiteral("true") : QStringLiteral("false"));
}

void PillButton::setCompact(bool compact)
{
    setDynamicProperty(this, "compact", compact ? QStringLiteral("true") : QStringLiteral("false"));
}

void PillButton::setIconResource(const QString &resourcePath, int size)
{
    m_iconPath = resourcePath;
    m_iconSize = size;
    setIconSize(QSize(size, size));
    refreshIcon();
}

void PillButton::refreshIcon()
{
    if (m_iconPath.isEmpty())
        return;
    const ThemeManager &theme = ThemeManager::instance();
    QColor tint;
    switch (m_variant) {
    case Primary:
        tint = theme.color(QStringLiteral("accentFg"), Qt::white);
        break;
    case Danger:
        tint = Qt::white;
        break;
    default:
        tint = theme.text();
        break;
    }
    setIcon(themedIcon(m_iconPath, tint, m_iconSize));
}

// ---------------------------------------------------------------------------
// Chip
// ---------------------------------------------------------------------------

Chip::Chip(const QString &text, QWidget *parent)
    : QPushButton(text, parent)
{
    setObjectName(QStringLiteral("chip"));
    setCheckable(true);
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::TabFocus);
}

// ---------------------------------------------------------------------------
// IconButton
// ---------------------------------------------------------------------------

IconButton::IconButton(const QString &resourcePath, const QString &tooltip, int iconSize,
                       QWidget *parent)
    : QToolButton(parent)
    , m_resourcePath(resourcePath)
    , m_token(QStringLiteral("textSecondary"))
    , m_iconSize(iconSize)
{
    setCursor(Qt::PointingHandCursor);
    setToolTip(tooltip);
    setIconSize(QSize(iconSize, iconSize));
    setFixedSize(iconSize + 14, iconSize + 14);
    setFocusPolicy(Qt::TabFocus);
    refreshIcon();

    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this,
            [this](bool) { refreshIcon(); });
}

void IconButton::setIconResource(const QString &resourcePath)
{
    m_resourcePath = resourcePath;
    refreshIcon();
}

void IconButton::setColorToken(const QString &token)
{
    m_token = token;
    refreshIcon();
}

void IconButton::refreshIcon()
{
    if (m_resourcePath.isEmpty())
        return;
    const QColor tint = ThemeManager::instance().color(m_token, ThemeManager::instance().textSecondary());
    setIcon(themedIcon(m_resourcePath, tint, m_iconSize));
}

} // namespace das::ui
