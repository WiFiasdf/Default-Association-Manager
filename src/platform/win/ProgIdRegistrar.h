#pragma once

#include <QString>

namespace das::win {

/// ProgId 注册器。
///
/// 当用户选择的可执行文件没有现成的 ProgId 时，在 HKCU\Software\Classes 下自建一个，
/// 并登记到扩展名的 OpenWithProgids 列表。全程只写 HKCU，无需提权。
class ProgIdRegistrar
{
public:
    struct Result {
        bool    ok = false;
        QString progId;
        QString message;
        long    winError = 0;
    };

    /// 为 exe 创建（或复用）一个 ProgId，并确保它出现在 target 的候选列表中。
    /// \param exePath 可执行文件绝对路径
    /// \param target  ".png" 或 "http"
    static Result ensureProgId(const QString &exePath, const QString &target, bool isProtocol);

    /// 生成本工具专用的 ProgId 名称，形如 "DefaultAppSetter.notepad.png"。
    static QString makeProgIdName(const QString &exePath, const QString &target);

    /// 检查 ProgId 是否已经存在可用的 shell\open\command。
    static bool progIdUsable(const QString &progId);
};

} // namespace das::win
