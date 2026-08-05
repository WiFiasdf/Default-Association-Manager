#pragma once

#include <QColor>
#include <QHash>
#include <QObject>
#include <QString>

namespace das {

/// 主题模式。System 跟随系统 colorScheme。
enum class ThemeMode {
    System = 0,
    Light,
    Dark
};

/// 全局主题管理：加载令牌化 QSS、跟随系统明暗、向绘制代码提供配色。
///
/// QSS 本身没有变量，这里用 `@token` 占位 + 运行时替换实现主题切换：
///   - `qss/common.qss` 只描述几何与状态
///   - `qss/light.qss` / `qss/dark.qss` 是纯令牌表
class ThemeManager : public QObject
{
    Q_OBJECT

public:
    static ThemeManager &instance();

    /// 读取用户偏好、连接系统主题变化并首次应用样式。可重复调用（幂等）。
    void initialize();

    ThemeMode mode() const { return m_mode; }
    void      setMode(ThemeMode mode);

    /// 在浅色 / 深色之间切换（会把模式固定为 Light 或 Dark）。
    void toggle();

    bool isDark() const { return m_dark; }

    /// 取主题令牌的颜色值，如 "accent"、"textSecondary"、"navSelected"。
    QColor color(const QString &token, const QColor &fallback = QColor()) const;

    /// 取主题令牌的原始字符串（图标路径类令牌用）。
    QString rawToken(const QString &token) const;

    QColor accent() const;
    QColor text() const;
    QColor textSecondary() const;
    QColor textTertiary() const;

    /// 当前模式的中文名称，用于界面提示。
    QString modeDisplayName() const;

signals:
    /// 主题已切换。所有自绘控件应在此重建颜色 / 图标缓存。
    void themeChanged(bool dark);

private:
    explicit ThemeManager(QObject *parent = nullptr);

    void        apply();
    bool        resolveDark() const;
    static bool systemPrefersDark();
    static QString readResource(const QString &path);
    static QHash<QString, QString> parseTokens(const QString &text);
    QString     substitute(const QString &qss) const;
    void        applyPalette();

    ThemeMode               m_mode        = ThemeMode::System;
    bool                    m_dark        = false;
    bool                    m_initialized = false;
    QHash<QString, QString> m_tokens;
    QStringList             m_tokenKeysByLength; ///< 长度降序，避免前缀误替换
};

} // namespace das
