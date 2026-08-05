#pragma once

#include <QString>

#include <windows.h>

namespace das::win {

/// 当前登录用户的 SID 字符串（形如 S-1-5-21-...），失败返回空串。结果内部缓存。
QString currentUserSid();

/// 真实的 Windows 内部版本号。使用 ntdll!RtlGetVersion，绕开清单兼容性谎报。
quint32 realBuildNumber();

/// 是否为 Windows 11（Build >= 22000）。
bool isWindows11OrGreater();

/// 把 FILETIME 截断到「分钟」精度（秒与毫秒清零）。
quint64 truncateFileTimeToMinute(quint64 fileTime);

/// 按 UserChoice 算法要求，把 FILETIME 格式化为 16 位小写十六进制（高 32 位在前）。
QString formatFileTimeHex(quint64 fileTime);

/// 通知资源管理器文件关联已变更。
void notifyAssociationChanged();

/// 把 Win32 错误码翻译成可读中文/系统消息。
QString formatWinError(long code);

/// 展开 %ProgramFiles% 之类的环境变量。
QString expandEnvironmentString(const QString &value);

/// 从形如 `"C:\a\b.exe" "%1"` 的命令行中提取可执行文件路径。
QString extractExecutableFromCommand(const QString &command);

} // namespace das::win
