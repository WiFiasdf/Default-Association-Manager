#pragma once

#include "core/Types.h"

#include <QDateTime>
#include <QList>
#include <QObject>
#include <QString>

namespace das {

/// 快照元信息。
struct BackupInfo {
    QString   filePath;
    QDateTime createdAt;
    int       entryCount = 0;
    QString   source;      ///< "自动" / "手动"
    QString   note;

    QString displayName() const;
};

using BackupInfoList = QList<BackupInfo>;

/// 备份服务：关联快照的创建、列举、读取与清理。
///
/// 快照只记录「目标 → ProgId」，**不存 Hash**：Hash 与写入时间戳强绑定，
/// 还原时必须按新的时间戳重算。
class BackupService : public QObject
{
    Q_OBJECT

public:
    static constexpr int kSchemaVersion = 1;
    static constexpr int kMaxBackups    = 30;

    explicit BackupService(QObject *parent = nullptr);

    /// 用给定条目创建快照。entries 为空时返回失败。
    bool createSnapshot(const ProfileEntryList &entries, const QString &source,
                        const QString &note, QString *outPath = nullptr);

    /// 列出全部快照，按时间倒序。
    BackupInfoList listSnapshots() const;

    /// 读取快照内容。
    ProfileEntryList loadSnapshot(const QString &filePath, QString *error = nullptr) const;

    /// 删除快照。
    bool removeSnapshot(const QString &filePath);

    /// 超出上限时清理最旧的快照。
    void pruneOldSnapshots();

signals:
    void snapshotsChanged();
};

} // namespace das
