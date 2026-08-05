#pragma once

#include "core/Types.h"
#include "services/BackupService.h"
#include "services/ProfileService.h"

#include <QHash>
#include <QList>
#include <QObject>
#include <QPair>

#include <memory>

class QThread;
class QWidget;

namespace das {

namespace win {
class AssocReader;
}

class HybridAssociationSetter;
class ScanWorker;

/// 业务门面：聚合读取器、混合设置器、备份与方案服务。
///
/// 所有写操作都在主线程执行（COM STA 要求，且系统对话框必须由 UI 线程弹出）；
/// 全量扫描在独立线程中进行。
class AssociationService : public QObject
{
    Q_OBJECT

public:
    explicit AssociationService(QObject *parent = nullptr);
    ~AssociationService() override;

    // --- 扫描 ---------------------------------------------------------------
    void startScan();
    void cancelScan();
    bool isScanning() const { return m_scanning; }

    const AssociationEntryList &entries() const { return m_entries; }
    AssociationEntry            entryFor(const QString &target, bool isProtocol) const;
    AssociationEntry            reload(const QString &target, bool isProtocol);

    // --- 读取 ---------------------------------------------------------------
    AppCandidateList candidatesFor(const QString &target, bool isProtocol);
    win::AssocReader &reader() { return *m_reader; }

    /// 为任意 exe 生成可用的 ProgId（必要时自建）。
    QString ensureProgIdForExe(const QString &exePath, const QString &target, bool isProtocol,
                               QString *error = nullptr);

    // --- 写入 ---------------------------------------------------------------
    SetResult     applyOne(const QString &target, bool isProtocol, const QString &progId,
                           QWidget *parent);
    SetResultList applyBatch(const TargetRefList &targets, const QString &progId, QWidget *parent);
    SetResult     restoreDefault(const QString &target, bool isProtocol);
    SetResultList restoreDefaults(const TargetRefList &targets);
    SetResultList applyProfile(const ProfileDiffList &diffs);

    /// 结果面板中的「去设置」：为单项手动触发系统引导。
    SetResult runGuided(const SetResult &item, QWidget *parent);

    /// 回滚到快照。
    SetResultList rollbackTo(const ProfileEntryList &entries);

    // --- 备份 / 方案 --------------------------------------------------------
    BackupService  *backupService() { return m_backup; }
    ProfileService *profileService() { return m_profile; }

    /// 当前全部关联组成的方案（用于导出与手动备份）。
    ProfileEntryList currentProfile() const;

    /// 为指定目标集合创建写前快照。
    void snapshotTargets(const TargetRefList &targets, const QString &note);

signals:
    void scanStarted(int total);
    void scanBatch(const AssociationEntryList &batch);
    void scanFinished(int total, qint64 elapsedMs, bool cancelled);
    void entryChanged(const AssociationEntry &entry);
    void batchProgress(int done, int total, const SetResult &last);

private slots:
    void onBatchReady(const AssociationEntryList &batch);
    void onScanFinished(int total, qint64 elapsedMs, bool cancelled);

private:
    void      refreshEntryAfterWrite(const QString &target, bool isProtocol);
    QString   indexKey(const QString &target, bool isProtocol) const;

    std::unique_ptr<win::AssocReader>        m_reader;
    std::unique_ptr<HybridAssociationSetter> m_setter;

    BackupService  *m_backup  = nullptr;
    ProfileService *m_profile = nullptr;

    QThread    *m_scanThread = nullptr;
    ScanWorker *m_worker     = nullptr;
    bool        m_scanning   = false;

    AssociationEntryList m_entries;
    QHash<QString, int>  m_index; ///< key -> m_entries 下标
};

} // namespace das
