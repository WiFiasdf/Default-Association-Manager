#pragma once

#include <QString>

namespace das::win {

/// UserChoice 哈希算法（微软未公开，社区逆向实现）。
///
/// 本文件内全部为**纯函数**，不触碰注册表、不依赖任何全局状态，因此可被单元测试完整覆盖。
///
/// 算法概要：
///   baseInfo = (扩展名/协议 + 用户SID + ProgId + 分钟级时间戳十六进制 + 固定盐值).toLower()
///   → UTF-16LE 编码（含结尾 NUL）
///   → MD5
///   → 两轮 32 位乘加置乱
///   → 两轮结果按 DWORD 互相异或得到 8 字节
///   → Base64
namespace userchoice {

/// UserChoice 算法使用的公开固定盐值。
QString experienceSalt();

/// 拼装参与哈希的原始输入串（已转小写）。
/// \param target    文件扩展名（含前导点，如 ".png"）或协议名（如 "http"，不带冒号）
/// \param userSid   当前用户 SID
/// \param progId    目标 ProgId
/// \param fileTime  UserChoice 键的 LastWriteTime（FILETIME 64 位，**必须已截断到分钟**）
QString buildBaseInfo(const QString &target, const QString &userSid, const QString &progId,
                      quint64 fileTime);

/// 对已拼装好的输入串求哈希，返回 Base64 字符串。输入非法时返回空串。
QString hashBaseInfo(const QString &baseInfo);

/// 一步到位：拼装 + 求哈希。
QString compute(const QString &target, const QString &userSid, const QString &progId,
                quint64 fileTime);

} // namespace userchoice
} // namespace das::win
