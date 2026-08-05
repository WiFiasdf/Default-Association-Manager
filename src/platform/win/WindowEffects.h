#pragma once

class QColor;
class QWidget;

namespace das::win {

/// Win11 窗口视觉增强。所有调用在旧系统 / 失败时静默降级，不影响启动。
namespace effects {

/// 圆角窗口（Win11 22000+）。
void applyRoundedCorners(QWidget *widget);

/// 深色标题栏。
void applyDarkTitleBar(QWidget *widget, bool dark);

/// 让系统标题栏与应用内配色一致（Win11 22000+），使原生标题栏与内容无缝衔接。
/// 传入无效颜色表示保持系统默认。
void applyCaptionColors(QWidget *widget, const QColor &caption, const QColor &text,
                        const QColor &border);

/// Mica 云母背景（Win11 22621+）。返回是否成功启用。
bool applyMicaBackdrop(QWidget *widget, bool enable);

/// 一次性应用全部效果，返回 Mica 是否生效（用于决定 QSS 是否绘制渐变兜底背景）。
bool applyAll(QWidget *widget, bool dark);

} // namespace effects
} // namespace das::win
