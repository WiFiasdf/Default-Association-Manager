#pragma once

#include "core/Types.h"

#include <QObject>
#include <QString>

namespace das {

namespace win {
class AssocReader;
}

/// 方案导入导出服务。与备份共用同一套 JSON Schema，仅 kind 字段不同。
class ProfileService : public QObject
{
    Q_OBJECT

public:
    static constexpr int kSchemaVersion = 1;

    explicit ProfileService(QObject *parent = nullptr);

    /// 导出方案到 JSON。
    bool exportProfile(const QString &filePath, const ProfileEntryList &entries,
                       QString *error = nullptr) const;

    /// 从 JSON 读取方案（同时兼容备份快照文件）。
    ProfileEntryList importProfile(const QString &filePath, QString *error = nullptr) const;

    /// 把方案与当前系统状态比对，生成差异列表（仅返回存在差异的项）。
    ProfileDiffList diffAgainstCurrent(const ProfileEntryList &entries,
                                       win::AssocReader &reader) const;
};

} // namespace das
