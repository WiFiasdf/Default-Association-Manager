#include "ui/ThemeManager.h"

#include "core/Logging.h"

#include <QApplication>
#include <QFile>
#include <QPalette>
#include <QRegularExpression>
#include <QSettings>
#include <QStringList>
#include <QStyleHints>
#include <QTextStream>

#include <algorithm>

namespace das {

namespace {

constexpr const char *kSettingsKeyTheme = "ui/themeMode";

ThemeMode modeFromString(const QString &value)
{
    if (value.compare(QLatin1String("light"), Qt::CaseInsensitive) == 0)
        return ThemeMode::Light;
    if (value.compare(QLatin1String("dark"), Qt::CaseInsensitive) == 0)
        return ThemeMode::Dark;
    return ThemeMode::System;
}

QString modeToString(ThemeMode mode)
{
    switch (mode) {
    case ThemeMode::Light:
        return QStringLiteral("light");
    case ThemeMode::Dark:
        return QStringLiteral("dark");
    case ThemeMode::System:
        break;
    }
    return QStringLiteral("system");
}

} // namespace

ThemeManager &ThemeManager::instance()
{
    static ThemeManager manager;
    return manager;
}

ThemeManager::ThemeManager(QObject *parent)
    : QObject(parent)
{
}

void ThemeManager::initialize()
{
    if (m_initialized)
        return;
    m_initialized = true;

    QSettings settings;
    m_mode = modeFromString(settings.value(QLatin1String(kSettingsKeyTheme)).toString());
    m_dark = resolveDark();

    if (QStyleHints *hints = QApplication::styleHints()) {
        connect(hints, &QStyleHints::colorSchemeChanged, this, [this](Qt::ColorScheme) {
            if (m_mode != ThemeMode::System)
                return;
            const bool dark = resolveDark();
            if (dark == m_dark)
                return;
            m_dark = dark;
            apply();
            qCInfo(logUi) << "系统主题变化，已切换到" << (m_dark ? "深色" : "浅色");
            emit themeChanged(m_dark);
        });
    }

    apply();
    qCInfo(logUi) << "主题初始化完成：" << modeDisplayName();
}

void ThemeManager::setMode(ThemeMode mode)
{
    if (m_mode == mode)
        return;
    m_mode = mode;

    QSettings settings;
    settings.setValue(QLatin1String(kSettingsKeyTheme), modeToString(mode));

    m_dark = resolveDark();
    apply();
    // 即便明暗未变（例如从「跟随系统（深色）」切到「深色」），也通知一次，
    // 让依赖 modeDisplayName 的界面刷新提示文案。
    emit themeChanged(m_dark);
}

void ThemeManager::toggle()
{
    setMode(m_dark ? ThemeMode::Light : ThemeMode::Dark);
}

bool ThemeManager::resolveDark() const
{
    switch (m_mode) {
    case ThemeMode::Light:
        return false;
    case ThemeMode::Dark:
        return true;
    case ThemeMode::System:
        break;
    }
    return systemPrefersDark();
}

bool ThemeManager::systemPrefersDark()
{
    if (QStyleHints *hints = QApplication::styleHints())
        return hints->colorScheme() == Qt::ColorScheme::Dark;
    return false;
}

QString ThemeManager::readResource(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qCWarning(logUi) << "读取样式资源失败：" << path;
        return {};
    }
    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);
    return stream.readAll();
}

QHash<QString, QString> ThemeManager::parseTokens(const QString &text)
{
    QHash<QString, QString> tokens;
    static const QRegularExpression re(QStringLiteral("^\\s*@([A-Za-z0-9_]+)\\s*:\\s*([^;]+);"),
                                       QRegularExpression::MultilineOption);
    auto it = re.globalMatch(text);
    while (it.hasNext()) {
        const QRegularExpressionMatch m = it.next();
        tokens.insert(m.captured(1), m.captured(2).trimmed());
    }
    return tokens;
}

QString ThemeManager::substitute(const QString &qss) const
{
    QString result = qss;
    for (const QString &key : m_tokenKeysByLength)
        result.replace(QLatin1Char('@') + key, m_tokens.value(key));
    return result;
}

void ThemeManager::apply()
{
    const QString tokenText = readResource(m_dark ? QStringLiteral(":/qss/dark.qss")
                                                  : QStringLiteral(":/qss/light.qss"));
    m_tokens = parseTokens(tokenText);

    m_tokenKeysByLength = m_tokens.keys();
    // 先替换长键，避免 "@accent" 抢先命中 "@accentHover"
    std::sort(m_tokenKeysByLength.begin(), m_tokenKeysByLength.end(),
              [](const QString &a, const QString &b) {
                  if (a.size() != b.size())
                      return a.size() > b.size();
                  return a < b;
              });

    const QString common = readResource(QStringLiteral(":/qss/common.qss"));
    if (common.isEmpty() || m_tokens.isEmpty()) {
        qCWarning(logUi) << "样式表为空，界面将使用 Qt 默认外观";
        return;
    }

    applyPalette();
    qApp->setStyleSheet(substitute(common));
}

void ThemeManager::applyPalette()
{
    // QSS 覆盖不到的原生绘制（文本光标、原生对话框等）靠调色板兜底
    QPalette palette = qApp->palette();
    const QColor base       = color(QStringLiteral("surface"), m_dark ? QColor(0x20, 0x20, 0x20)
                                                                      : QColor(0xFB, 0xFB, 0xFB));
    const QColor windowText = color(QStringLiteral("text"), m_dark ? Qt::white : Qt::black);
    const QColor accentCol  = accent();

    palette.setColor(QPalette::Window, base);
    palette.setColor(QPalette::WindowText, windowText);
    palette.setColor(QPalette::Base, color(QStringLiteral("control"), base));
    palette.setColor(QPalette::AlternateBase, color(QStringLiteral("surfaceAlt"), base));
    palette.setColor(QPalette::Text, windowText);
    palette.setColor(QPalette::ButtonText, windowText);
    palette.setColor(QPalette::Button, color(QStringLiteral("control"), base));
    palette.setColor(QPalette::ToolTipBase, color(QStringLiteral("menuBg"), base));
    palette.setColor(QPalette::ToolTipText, windowText);
    palette.setColor(QPalette::Highlight, accentCol);
    palette.setColor(QPalette::HighlightedText, color(QStringLiteral("accentFg"), Qt::white));
    palette.setColor(QPalette::PlaceholderText, textTertiary());
    palette.setColor(QPalette::Link, accentCol);
    palette.setColor(QPalette::Disabled, QPalette::Text, color(QStringLiteral("textDisabled"),
                                                               QColor(0xA5, 0xA5, 0xA5)));
    palette.setColor(QPalette::Disabled, QPalette::WindowText,
                     color(QStringLiteral("textDisabled"), QColor(0xA5, 0xA5, 0xA5)));
    qApp->setPalette(palette);
}

QColor ThemeManager::color(const QString &token, const QColor &fallback) const
{
    const QString raw = m_tokens.value(token);
    if (raw.isEmpty())
        return fallback;
    const QColor parsed = QColor::fromString(raw);
    return parsed.isValid() ? parsed : fallback;
}

QString ThemeManager::rawToken(const QString &token) const
{
    return m_tokens.value(token);
}

QColor ThemeManager::accent() const
{
    return color(QStringLiteral("accent"), m_dark ? QColor(0x4C, 0xC2, 0xFF) : QColor(0x00, 0x67, 0xC0));
}

QColor ThemeManager::text() const
{
    return color(QStringLiteral("text"), m_dark ? QColor(0xFF, 0xFF, 0xFF) : QColor(0x1B, 0x1B, 0x1B));
}

QColor ThemeManager::textSecondary() const
{
    return color(QStringLiteral("textSecondary"),
                 m_dark ? QColor(0xC5, 0xC5, 0xC5) : QColor(0x5D, 0x5D, 0x5D));
}

QColor ThemeManager::textTertiary() const
{
    return color(QStringLiteral("textTertiary"),
                 m_dark ? QColor(0x9C, 0x9C, 0x9C) : QColor(0x76, 0x76, 0x76));
}

QString ThemeManager::modeDisplayName() const
{
    switch (m_mode) {
    case ThemeMode::Light:
        return QStringLiteral("浅色");
    case ThemeMode::Dark:
        return QStringLiteral("深色");
    case ThemeMode::System:
        break;
    }
    return m_dark ? QStringLiteral("跟随系统（深色）") : QStringLiteral("跟随系统（浅色）");
}

} // namespace das
