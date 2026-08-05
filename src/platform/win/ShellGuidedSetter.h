#pragma once

#include <QString>

class QWidget;

namespace das::win {

/// 系统引导回退路径：调用文档化的 Shell API，让用户手动点一次完成关联。
class ShellGuidedSetter
{
public:
    /// 弹出系统「打开方式」对话框（模态）。
    /// \return 用户是否完成了选择（不代表一定选中了我们期望的程序）
    static bool openWithDialog(QWidget *parent, const QString &extension);

    /// 打开「设置 → 默认应用」，可定位到指定已注册应用。
    static bool openDefaultAppsSettings(const QString &registeredAppName = QString());

    /// 打开「设置 → 默认应用 → 按文件类型」。
    static bool openDefaultAppsByFileType();

    /// 打开「设置 → 默认应用 → 按协议」。
    static bool openDefaultAppsByProtocol();

    /// 根据 ProgId 反查它属于哪个 RegisteredApplications 条目，便于深链定位。
    static QString registeredAppNameForProgId(const QString &progId);
};

} // namespace das::win
