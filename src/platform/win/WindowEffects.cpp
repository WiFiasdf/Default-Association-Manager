#include "platform/win/WindowEffects.h"

#include "core/Logging.h"
#include "platform/win/WinUtils.h"

#include <QColor>
#include <QWidget>
#include <QWindow>

#include <windows.h>
#include <dwmapi.h>

// 旧 SDK 兜底定义（当前 SDK 10.0.26100 已全部包含，这里保证向下兼容）
#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#  define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif
#ifndef DWMWA_WINDOW_CORNER_PREFERENCE
#  define DWMWA_WINDOW_CORNER_PREFERENCE 33
#endif
#ifndef DWMWA_SYSTEMBACKDROP_TYPE
#  define DWMWA_SYSTEMBACKDROP_TYPE 38
#endif
#ifndef DWMWA_BORDER_COLOR
#  define DWMWA_BORDER_COLOR 34
#endif
#ifndef DWMWA_CAPTION_COLOR
#  define DWMWA_CAPTION_COLOR 35
#endif
#ifndef DWMWA_TEXT_COLOR
#  define DWMWA_TEXT_COLOR 36
#endif

namespace das::win::effects {

namespace {

constexpr DWORD kCornerRound   = 2; // DWMWCP_ROUND
constexpr DWORD kBackdropMica  = 2; // DWMSBT_MAINWINDOW
constexpr DWORD kBackdropAuto  = 0; // DWMSBT_AUTO

HWND handleOf(QWidget *widget)
{
    if (!widget)
        return nullptr;
    // 确保原生窗口已创建
    widget->winId();
    QWindow *window = widget->windowHandle();
    if (!window)
        return nullptr;
    return reinterpret_cast<HWND>(window->winId());
}

bool setAttr(HWND hwnd, DWORD attribute, const void *data, DWORD size)
{
    if (!hwnd)
        return false;
    const HRESULT hr = ::DwmSetWindowAttribute(hwnd, attribute, data, size);
    return SUCCEEDED(hr);
}

} // namespace

void applyRoundedCorners(QWidget *widget)
{
    if (realBuildNumber() < 22000u)
        return;
    HWND hwnd = handleOf(widget);
    const DWORD preference = kCornerRound;
    if (!setAttr(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &preference, sizeof(preference)))
        qCDebug(logUi) << "设置窗口圆角失败，已降级";
}

void applyDarkTitleBar(QWidget *widget, bool dark)
{
    HWND hwnd = handleOf(widget);
    const BOOL value = dark ? TRUE : FALSE;
    if (!setAttr(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &value, sizeof(value)))
        qCDebug(logUi) << "设置深色标题栏失败，已降级";
}

void applyCaptionColors(QWidget *widget, const QColor &caption, const QColor &text,
                        const QColor &border)
{
    if (realBuildNumber() < 22000u)
        return;
    HWND hwnd = handleOf(widget);
    if (!hwnd)
        return;

    const auto toColorRef = [](const QColor &color) -> COLORREF {
        return RGB(color.red(), color.green(), color.blue());
    };

    if (caption.isValid()) {
        const COLORREF value = toColorRef(caption);
        setAttr(hwnd, DWMWA_CAPTION_COLOR, &value, sizeof(value));
    }
    if (text.isValid()) {
        const COLORREF value = toColorRef(text);
        setAttr(hwnd, DWMWA_TEXT_COLOR, &value, sizeof(value));
    }
    if (border.isValid()) {
        const COLORREF value = toColorRef(border);
        setAttr(hwnd, DWMWA_BORDER_COLOR, &value, sizeof(value));
    }
}

bool applyMicaBackdrop(QWidget *widget, bool enable)
{
    if (realBuildNumber() < 22621u)
        return false;
    HWND hwnd = handleOf(widget);
    const DWORD backdrop = enable ? kBackdropMica : kBackdropAuto;
    const bool  ok = setAttr(hwnd, DWMWA_SYSTEMBACKDROP_TYPE, &backdrop, sizeof(backdrop));
    if (!ok)
        qCDebug(logUi) << "启用 Mica 背景失败，已降级为 QSS 渐变";
    return ok && enable;
}

bool applyAll(QWidget *widget, bool dark)
{
    applyRoundedCorners(widget);
    applyDarkTitleBar(widget, dark);
    // Mica 需要窗口背景透明才能透出，Widgets 场景下容易出现文字发虚，
    // 因此这里只在 Win11 22621+ 尝试，失败即用 QSS 渐变兜底。
    return applyMicaBackdrop(widget, false);
}

} // namespace das::win::effects
