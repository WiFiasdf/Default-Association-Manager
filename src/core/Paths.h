#pragma once

#include <QString>

namespace das {

/// 应用数据根目录（%APPDATA%/DefaultAppSetter），必要时自动创建。
QString appDataDir();

/// 备份快照目录。
QString backupDir();

/// 日志目录。
QString logDir();

/// 主日志文件完整路径。
QString logFilePath();

} // namespace das
