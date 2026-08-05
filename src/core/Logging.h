#pragma once

#include <QLoggingCategory>
#include <QString>

Q_DECLARE_LOGGING_CATEGORY(logReg)
Q_DECLARE_LOGGING_CATEGORY(logAssoc)
Q_DECLARE_LOGGING_CATEGORY(logUi)

namespace das {

/// 安装文件日志处理器（2MB 滚动，保留 3 份）。
void installLogging();

/// 卸载日志处理器并关闭文件。
void shutdownLogging();

/// SID 脱敏：仅保留末尾 4 个字符。
QString redactSid(const QString &sid);

/// 哈希脱敏：仅保留前 4 个字符。
QString redactHash(const QString &hash);

} // namespace das
