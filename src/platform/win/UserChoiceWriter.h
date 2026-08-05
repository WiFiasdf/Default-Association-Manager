#pragma once

#include <QString>

namespace das::win {

/// UserChoice 静默写入流程编排。
///
/// 只写 HKCU，无需管理员权限。写入本身不保证生效 —— 调用方必须回读校验。
class UserChoiceWriter
{
public:
    struct Outcome {
        bool    written  = false;
        long    winError = 0;
        QString message;
        QString hash; ///< 已写入的哈希（记录日志前需脱敏）
    };

    /// 计算并写入 UserChoice。target 为 ".png" 或 "http"。
    static Outcome write(const QString &target, bool isProtocol, const QString &progId);

    /// 删除 UserChoice，让系统回落到默认关联。
    static Outcome remove(const QString &target, bool isProtocol);

    /// 关联键的父路径（HKCU 下）。
    static QString parentKeyPath(const QString &target, bool isProtocol);
};

} // namespace das::win
