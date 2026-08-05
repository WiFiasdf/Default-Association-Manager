#include "platform/win/ShellGuidedSetter.h"

#include "core/Logging.h"
#include "platform/win/RegKey.h"
#include "platform/win/WinUtils.h"

#include <QWidget>
#include <QWindow>

#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>

#ifndef OAIF_FORCE_OPEN_WITH
#  define OAIF_FORCE_OPEN_WITH 0x00040000
#endif
#ifndef OAIF_HIDE_REGISTRATION
#  define OAIF_HIDE_REGISTRATION 0x00080000
#endif

namespace das::win {

namespace {

HWND nativeHandle(QWidget *widget)
{
    if (!widget)
        return ::GetActiveWindow();
    widget->winId();
    if (QWindow *window = widget->window()->windowHandle())
        return reinterpret_cast<HWND>(window->winId());
    return ::GetActiveWindow();
}

bool launchUri(const QString &uri)
{
    const HINSTANCE result = ::ShellExecuteW(nullptr, L"open", RegKey::wstr(uri),
                                             nullptr, nullptr, SW_SHOWNORMAL);
    const auto code = reinterpret_cast<INT_PTR>(result);
    const bool ok   = code > 32;
    if (!ok)
        qCWarning(logAssoc) << "打开系统设置失败:" << uri << "code=" << code;
    return ok;
}

} // namespace

bool ShellGuidedSetter::openWithDialog(QWidget *parent, const QString &extension)
{
    if (extension.isEmpty())
        return false;

    // 一个不需要真实存在的示例文件名，仅用于让 Shell 推断类型
    const QString sample = QStringLiteral("sample") + extension;

    OPENASINFO info{};
    info.pcszFile  = RegKey::wstr(sample);
    info.pcszClass = RegKey::wstr(extension);
    info.oaifInFlags = OAIF_REGISTER_EXT | OAIF_FORCE_OPEN_WITH | OAIF_HIDE_REGISTRATION;

    const HRESULT hr = ::SHOpenWithDialog(nativeHandle(parent), &info);
    if (hr == S_OK)
        return true;

    // 用户取消
    if (hr == HRESULT_FROM_WIN32(ERROR_CANCELLED))
        return false;

    qCWarning(logAssoc) << "SHOpenWithDialog 失败, ext=" << extension << "hr=" << Qt::hex << hr;
    return false;
}

bool ShellGuidedSetter::openDefaultAppsSettings(const QString &registeredAppName)
{
    if (!registeredAppName.isEmpty()) {
        return launchUri(QStringLiteral("ms-settings:defaultapps?registeredAppUser=%1")
                             .arg(registeredAppName));
    }
    return launchUri(QStringLiteral("ms-settings:defaultapps"));
}

bool ShellGuidedSetter::openDefaultAppsByFileType()
{
    return launchUri(QStringLiteral("ms-settings:defaultapps?registeredAppsFileType"));
}

bool ShellGuidedSetter::openDefaultAppsByProtocol()
{
    return launchUri(QStringLiteral("ms-settings:defaultapps?registeredAppsProtocol"));
}

QString ShellGuidedSetter::registeredAppNameForProgId(const QString &progId)
{
    if (progId.isEmpty())
        return {};

    for (HKEY root : {HKEY_CURRENT_USER, HKEY_LOCAL_MACHINE}) {
        RegKey regApps = RegKey::open(root, QStringLiteral("SOFTWARE\\RegisteredApplications"), KEY_READ);
        if (!regApps.isValid())
            continue;

        const QStringList appNames = regApps.valueNames();
        for (const QString &appName : appNames) {
            bool          ok            = false;
            const QString capabilityKey = regApps.readString(appName, &ok);
            if (!ok || capabilityKey.isEmpty())
                continue;

            for (const QString &suffix : {QStringLiteral("\\FileAssociations"),
                                          QStringLiteral("\\URLAssociations")}) {
                RegKey assoc = RegKey::open(root, capabilityKey + suffix, KEY_READ);
                if (!assoc.isValid())
                    continue;
                const QStringList targets = assoc.valueNames();
                for (const QString &target : targets) {
                    bool          valueOk = false;
                    const QString value   = assoc.readString(target, &valueOk);
                    if (valueOk && value.compare(progId, Qt::CaseInsensitive) == 0)
                        return appName;
                }
            }
        }
    }
    return {};
}

} // namespace das::win
