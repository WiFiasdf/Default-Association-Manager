#pragma once

#include "core/Types.h"
#include "ui/PageBase.h"

class QLabel;
class QWidget;

namespace das {

class AssociationService;

namespace ui {

class PillButton;

/// 备份还原页：快照时间线、回滚、恢复系统默认、方案导入导出。
class BackupPage : public PageBase
{
    Q_OBJECT

public:
    explicit BackupPage(AssociationService *service, QWidget *parent = nullptr);

    void onActivated() override;
    void setCompactMode(bool compact) override;

private slots:
    void onBackupNow();
    void onExport();
    void onImport();
    void onRefreshSnapshots();
    void onViewSnapshot(const QString &filePath);
    void onRollbackSnapshot(const QString &filePath);
    void onRecoverSelected();

private:
    void buildSnapshotList();
    ProfileEntryList currentEntries() const;
    void applyDiff(const ProfileDiffList &diffs);

    AssociationService *m_service = nullptr;
    QWidget             *m_snapList = nullptr;
    QLabel              *m_emptyHint = nullptr;
    bool                 m_compact = false;
};

} // namespace ui
} // namespace das
