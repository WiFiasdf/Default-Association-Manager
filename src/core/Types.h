#pragma once

#include <QList>
#include <QMetaType>
#include <QPair>
#include <QString>

namespace das {

/// 文件类型的粗分类，用于界面筛选芯片。
enum class FileCategory {
    All = 0,
    Image,
    Video,
    Audio,
    Document,
    Code,
    Archive,
    Executable,
    Other
};

/// 关联最终是通过哪条路径落地的。
enum class SetMethod {
    None = 0,
    Silent,  ///< UserChoice 哈希静默直写
    Guided   ///< 系统「打开方式」对话框 / ms-settings 引导
};

/// 单次设置的结果状态。
enum class SetStatus {
    Success = 0,        ///< 已生效（回读校验通过）
    NeedsUserConfirm,   ///< 已弹出系统界面，等待用户手动确认
    Failed,             ///< 失败
    Skipped             ///< 目标已经是期望值，无需改动
};

/// 一条关联记录（文件扩展名或 URL 协议）。
struct AssociationEntry {
    QString      target;                            ///< ".png" 或 "http"
    bool         isProtocol = false;
    QString      progId;                            ///< 当前默认 ProgId
    QString      appName;                           ///< 当前默认程序的友好名称
    QString      appPath;                           ///< 当前默认程序的可执行文件路径
    QString      typeDescription;                   ///< 文件类型说明，如「PNG 图像」
    FileCategory category = FileCategory::Other;

    bool isValid() const { return !target.isEmpty(); }
};

/// 可供选择的候选程序。
struct AppCandidate {
    QString progId;
    QString displayName;
    QString exePath;
    bool    isCurrent = false;

    bool operator==(const AppCandidate &other) const
    {
        return progId.compare(other.progId, Qt::CaseInsensitive) == 0;
    }
};

/// 一次设置操作的结果。
struct SetResult {
    QString   target;
    QString   progId;
    QString   appName;
    bool      isProtocol = false;
    SetMethod method     = SetMethod::None;
    SetStatus status     = SetStatus::Failed;
    QString   message;      ///< 面向用户的中文描述
    long      winError = 0; ///< 原始 Win32 / HRESULT 错误码，仅写日志

    bool ok() const { return status == SetStatus::Success || status == SetStatus::Skipped; }
};

using AssociationEntryList = QList<AssociationEntry>;
using AppCandidateList     = QList<AppCandidate>;
using SetResultList        = QList<SetResult>;

/// 目标标识：<名称, 是否为协议>。名称为扩展名（含点）或协议名（不含冒号）。
using TargetRef     = QPair<QString, bool>;
using TargetRefList = QList<TargetRef>;

/// 备份 / 方案中的一条记录。
struct ProfileEntry {
    QString target;
    QString progId;
    bool    isProtocol = false;
};

using ProfileEntryList = QList<ProfileEntry>;

/// 导入方案与当前系统状态的差异。
struct ProfileDiff {
    QString target;
    bool    isProtocol = false;
    QString currentProgId;
    QString currentAppName;
    QString targetProgId;
    QString targetAppName;
    bool    selected = true;

    bool changed() const
    {
        return currentProgId.compare(targetProgId, Qt::CaseInsensitive) != 0;
    }
};

using ProfileDiffList = QList<ProfileDiff>;

QString categoryDisplayName(FileCategory category);
QString setStatusDisplayName(SetStatus status);
QString setMethodDisplayName(SetMethod method);

} // namespace das

Q_DECLARE_METATYPE(das::AssociationEntry)
Q_DECLARE_METATYPE(das::AssociationEntryList)
Q_DECLARE_METATYPE(das::AppCandidate)
Q_DECLARE_METATYPE(das::SetResult)
Q_DECLARE_METATYPE(das::SetResultList)
Q_DECLARE_METATYPE(das::ProfileEntry)
