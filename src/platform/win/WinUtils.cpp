#include "platform/win/WinUtils.h"

#include "core/Logging.h"

#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QVarLengthArray>

#include <sddl.h>
#include <shlobj.h>
#include <shlwapi.h>

namespace das::win {

namespace {

QString querySidSlow()
{
    HANDLE token = nullptr;
    if (!::OpenProcessToken(::GetCurrentProcess(), TOKEN_QUERY, &token))
        return {};

    DWORD size = 0;
    ::GetTokenInformation(token, TokenUser, nullptr, 0, &size);
    if (size == 0) {
        ::CloseHandle(token);
        return {};
    }

    QVarLengthArray<BYTE, 256> buffer(static_cast<qsizetype>(size));
    QString                    result;
    if (::GetTokenInformation(token, TokenUser, buffer.data(), size, &size)) {
        auto    *tokenUser = reinterpret_cast<TOKEN_USER *>(buffer.data());
        LPWSTR   sidText   = nullptr;
        if (::ConvertSidToStringSidW(tokenUser->User.Sid, &sidText)) {
            result = QString::fromWCharArray(sidText);
            ::LocalFree(sidText);
        }
    }
    ::CloseHandle(token);
    return result;
}

using RtlGetVersionFn = LONG(WINAPI *)(PRTL_OSVERSIONINFOW);

} // namespace

QString currentUserSid()
{
    static const QString sid = querySidSlow();
    return sid;
}

quint32 realBuildNumber()
{
    static const quint32 build = []() -> quint32 {
        HMODULE ntdll = ::GetModuleHandleW(L"ntdll.dll");
        if (ntdll) {
            auto fn = reinterpret_cast<RtlGetVersionFn>(
                reinterpret_cast<void *>(::GetProcAddress(ntdll, "RtlGetVersion")));
            if (fn) {
                RTL_OSVERSIONINFOW info{};
                info.dwOSVersionInfoSize = sizeof(info);
                if (fn(&info) == 0)
                    return info.dwBuildNumber;
            }
        }
        // 兜底：从注册表读取
        bool    ok  = false;
        QString str = QString();
        HKEY    key = nullptr;
        if (::RegOpenKeyExW(HKEY_LOCAL_MACHINE,
                            L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", 0,
                            KEY_QUERY_VALUE, &key) == ERROR_SUCCESS) {
            wchar_t buf[64]{};
            DWORD   size = sizeof(buf);
            DWORD   type = 0;
            if (::RegQueryValueExW(key, L"CurrentBuildNumber", nullptr, &type,
                                   reinterpret_cast<BYTE *>(buf), &size) == ERROR_SUCCESS) {
                str = QString::fromWCharArray(buf);
            }
            ::RegCloseKey(key);
        }
        const quint32 value = str.toUInt(&ok);
        return ok ? value : 0u;
    }();
    return build;
}

bool isWindows11OrGreater()
{
    return realBuildNumber() >= 22000u;
}

quint64 truncateFileTimeToMinute(quint64 fileTime)
{
    FILETIME ft{};
    ft.dwLowDateTime  = static_cast<DWORD>(fileTime & 0xFFFFFFFFull);
    ft.dwHighDateTime = static_cast<DWORD>(fileTime >> 32);

    SYSTEMTIME st{};
    if (!::FileTimeToSystemTime(&ft, &st))
        return fileTime;

    st.wSecond       = 0;
    st.wMilliseconds = 0;

    FILETIME truncated{};
    if (!::SystemTimeToFileTime(&st, &truncated))
        return fileTime;

    return (static_cast<quint64>(truncated.dwHighDateTime) << 32) | truncated.dwLowDateTime;
}

QString formatFileTimeHex(quint64 fileTime)
{
    const quint32 high = static_cast<quint32>(fileTime >> 32);
    const quint32 low  = static_cast<quint32>(fileTime & 0xFFFFFFFFull);
    return QStringLiteral("%1%2")
        .arg(high, 8, 16, QLatin1Char('0'))
        .arg(low, 8, 16, QLatin1Char('0'))
        .toLower();
}

void notifyAssociationChanged()
{
    ::SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST | SHCNF_FLUSH, nullptr, nullptr);
}

QString formatWinError(long code)
{
    if (code == 0)
        return QStringLiteral("成功");

    LPWSTR buffer = nullptr;
    const DWORD len = ::FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr, static_cast<DWORD>(code), 0, reinterpret_cast<LPWSTR>(&buffer), 0, nullptr);

    QString message;
    if (len && buffer) {
        message = QString::fromWCharArray(buffer, static_cast<int>(len)).trimmed();
        ::LocalFree(buffer);
    }
    if (message.isEmpty())
        message = QStringLiteral("未知错误");
    return QStringLiteral("%1 (0x%2)").arg(message).arg(static_cast<quint32>(code), 8, 16, QLatin1Char('0'));
}

QString expandEnvironmentString(const QString &value)
{
    if (!value.contains(QLatin1Char('%')))
        return value;

    const auto *src  = reinterpret_cast<const wchar_t *>(value.utf16());
    DWORD       need = ::ExpandEnvironmentStringsW(src, nullptr, 0);
    if (need == 0)
        return value;

    QVarLengthArray<wchar_t, 512> buffer(static_cast<qsizetype>(need + 1));
    const DWORD written = ::ExpandEnvironmentStringsW(src, buffer.data(), need + 1);
    if (written == 0)
        return value;
    return QString::fromWCharArray(buffer.data());
}

QString extractExecutableFromCommand(const QString &command)
{
    QString cmd = expandEnvironmentString(command.trimmed());
    if (cmd.isEmpty())
        return {};

    QString path;
    if (cmd.startsWith(QLatin1Char('"'))) {
        const int closing = cmd.indexOf(QLatin1Char('"'), 1);
        path = (closing > 0) ? cmd.mid(1, closing - 1) : cmd.mid(1);
    } else {
        // 未加引号：尝试逐段扩展，直到找到存在的 .exe
        static const QRegularExpression sep(QStringLiteral("\\s+"));
        const QStringList parts = cmd.split(sep, Qt::SkipEmptyParts);
        QString           acc;
        for (const QString &part : parts) {
            acc = acc.isEmpty() ? part : acc + QLatin1Char(' ') + part;
            if (QFileInfo::exists(acc)) {
                path = acc;
                break;
            }
            if (acc.endsWith(QLatin1String(".exe"), Qt::CaseInsensitive)) {
                path = acc;
                break;
            }
        }
        if (path.isEmpty())
            path = parts.isEmpty() ? cmd : parts.first();
    }

    path = path.trimmed();
    if (path.isEmpty())
        return {};
    return QDir::toNativeSeparators(path);
}

} // namespace das::win
