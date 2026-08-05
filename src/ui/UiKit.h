#pragma once

#include <QColor>
#include <QFont>
#include <QFrame>
#include <QIcon>
#include <QPixmap>
#include <QPushButton>
#include <QString>
#include <QToolButton>

class QLabel;
class QWidget;

namespace das::ui {

// ---------------------------------------------------------------------------
// 矢量图标
// ---------------------------------------------------------------------------

/// 把资源中的 SVG 按指定颜色重新着色并栅格化（内部带缓存）。
/// SVG 中所有 `currentColor` 会被替换为 color。
QPixmap renderSvg(const QString &resourcePath, int size, const QColor &color, qreal dpr = 0.0);

/// 生成一个随主题着色的 QIcon（含 disabled 态）。
QIcon themedIcon(const QString &resourcePath, const QColor &color, int size = 20);

/// 清空图标栅格缓存（主题切换时调用）。
void clearIconCache();

// ---------------------------------------------------------------------------
// 样式辅助
// ---------------------------------------------------------------------------

/// 修改动态属性后重新应用 QSS。
void repolish(QWidget *widget);

/// 设置动态属性并 repolish。
void setDynamicProperty(QWidget *widget, const char *name, const QVariant &value);

/// 柔和投影。alpha 为 0 时移除效果。
void applyShadow(QWidget *widget, int blurRadius = 24, int alpha = 38, int dy = 4);

QLabel *makeLabel(const QString &text, const char *role, QWidget *parent = nullptr);
QLabel *makePageTitle(const QString &text, QWidget *parent = nullptr);
QLabel *makePageSubtitle(const QString &text, QWidget *parent = nullptr);
QLabel *makeSectionTitle(const QString &text, QWidget *parent = nullptr);
QLabel *makeCaption(const QString &text, QWidget *parent = nullptr);

QFrame *makeSeparator(Qt::Orientation orientation = Qt::Horizontal, QWidget *parent = nullptr);

/// 在基准字体上做相对缩放（deltaPt 以 pt 为单位，可为负）。
/// QSS 若用 `font-size: 14px` 指定字号，QFont::pointSizeF() 会返回 -1，
/// 直接加减会得到非法负值并触发 "QFont::setPointSizeF: Point size <= 0"，
/// 因此这里按实际度量单位分别处理。
QFont scaledFont(const QFont &base, qreal deltaPt, qreal minPt = 7.5);

// ---------------------------------------------------------------------------
// 控件
// ---------------------------------------------------------------------------

/// 圆角卡片容器。objectName 决定配色："card"（浮起）或 "cardFlat"（平铺）。
class Card : public QFrame
{
    Q_OBJECT

public:
    explicit Card(QWidget *parent = nullptr, bool flat = false, bool shadow = true);
};

/// 带变体的按钮：primary / secondary / subtle / danger，可选胶囊外形。
class PillButton : public QPushButton
{
    Q_OBJECT

public:
    enum Variant { Secondary, Primary, Subtle, Danger };

    explicit PillButton(const QString &text, Variant variant = Secondary, QWidget *parent = nullptr);

    void setVariant(Variant variant);
    void setPill(bool pill);
    void setCompact(bool compact);

    /// 让按钮显示一枚随主题着色的图标。
    void setIconResource(const QString &resourcePath, int size = 16);

private:
    void refreshIcon();

    Variant m_variant = Secondary;
    QString m_iconPath;
    int     m_iconSize = 16;
};

/// 类别筛选芯片（可选中）。
class Chip : public QPushButton
{
    Q_OBJECT

public:
    explicit Chip(const QString &text, QWidget *parent = nullptr);
};

/// 纯图标按钮，随主题自动重新着色。
class IconButton : public QToolButton
{
    Q_OBJECT

public:
    explicit IconButton(const QString &resourcePath, const QString &tooltip = QString(),
                        int iconSize = 18, QWidget *parent = nullptr);

    void setIconResource(const QString &resourcePath);
    /// 指定使用的主题令牌，默认 "textSecondary"。
    void setColorToken(const QString &token);

private:
    void refreshIcon();

    QString m_resourcePath;
    QString m_token;
    int     m_iconSize;
};

} // namespace das::ui
