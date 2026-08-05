#include "ui/pages/BackupPage.h"

#include "core/Logging.h"
#include "core/Paths.h"
#include "services/AssociationService.h"
#include "services/BackupService.h"
#include "services/ProfileService.h"
#include "ui/ThemeManager.h"
#include "ui/UiKit.h"

#include <QDateTime>
#include <QDesktopServices>
#include <QDialog>
#include <QDir>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

#include <algorithm>

namespace das::ui {

BackupPage::BackupPage(AssociationService *service, QWidget *parent)
    : PageBase(parent)
    , m_service(service)
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(24, 18, 24, 18);
    root->setSpacing(14);

    auto *head = new QHBoxLayout;
    head->setSpacing(16);
    auto *titles = new QVBoxLayout;
    titles->setSpacing(2);
    titles->addWidget(makeSectionTitle(QStringLiteral("备份与还原"), this));
    titles->addWidget(makeCaption(QStringLiteral("每次修改前会自动生成快照；可随时回滚或导出方案。"),
                                  this));
    head->addLayout(titles, 1);

    auto *backupBtn = new PillButton(QStringLiteral("立即备份"), PillButton::Primary, this);
    backupBtn->setIcon(QIcon(QStringLiteral(":/icons/app.svg")));
    connect(backupBtn, &QPushButton::clicked, this, &BackupPage::onBackupNow);
    head->addWidget(backupBtn, 0, Qt::AlignRight);

    auto *exportBtn = new PillButton(QStringLiteral("导出方案"), PillButton::Secondary, this);
    connect(exportBtn, &QPushButton::clicked, this, &BackupPage::onExport);
    head->addWidget(exportBtn, 0, Qt::AlignRight);

    auto *importBtn = new PillButton(QStringLiteral("导入方案"), PillButton::Secondary, this);
    connect(importBtn, &QPushButton::clicked, this, &BackupPage::onImport);
    head->addWidget(importBtn, 0, Qt::AlignRight);
    root->addLayout(head);

    // 快照时间线
    m_snapList = new QWidget(this);
    auto *snapLayout = new QVBoxLayout(m_snapList);
    snapLayout->setContentsMargins(0, 0, 0, 0);
    snapLayout->setSpacing(10);
    root->addWidget(m_snapList, 1);

    m_emptyHint = makeCaption(QStringLiteral("暂无快照，点击「立即备份」创建。"), this);
    root->addWidget(m_emptyHint);

    // 危险操作区
    auto *danger = new QWidget(this);
    danger->setObjectName(QStringLiteral("cardDanger"));
    auto *dangerLayout = new QVBoxLayout(danger);
    dangerLayout->setContentsMargins(16, 14, 16, 14);
    dangerLayout->setSpacing(10);
    dangerLayout->addWidget(makeCaption(QStringLiteral("危险操作：将选中的文件类型恢复为系统默认关联。"),
                                        danger));
    auto *recover = new PillButton(QStringLiteral("恢复所选为系统默认"), PillButton::Danger, danger);
    connect(recover, &QPushButton::clicked, this, &BackupPage::onRecoverSelected);
    dangerLayout->addWidget(recover, 0, Qt::AlignLeft);
    root->addWidget(danger);

    onRefreshSnapshots();
}

void BackupPage::onActivated()
{
    onRefreshSnapshots();
}

void BackupPage::setCompactMode(bool /*compact*/)
{
}

ProfileEntryList BackupPage::currentEntries() const
{
    return m_service->currentProfile();
}

void BackupPage::onBackupNow()
{
    const ProfileEntryList entries = currentEntries();
    if (entries.isEmpty()) {
        emit toastRequested(QStringLiteral("暂无可备份的关联，请先等待扫描完成"), 2600);
        return;
    }
    const bool ok = m_service->backupService()->createSnapshot(entries, QStringLiteral("手动"),
                                                               QString(), nullptr);
    if (!ok)
        emit toastRequested(QStringLiteral("备份失败，请查看日志"), 3200);
    else
        emit toastRequested(QStringLiteral("已创建快照"), 2600);
    onRefreshSnapshots();
}

void BackupPage::onExport()
{
    const QString dir = backupDir();
    const QString path = QFileDialog::getSaveFileName(
        this, QStringLiteral("导出关联方案"), dir + QStringLiteral("/profile.json"),
        QStringLiteral("JSON 方案 (*.json)"));
    if (path.isEmpty())
        return;

    QString err;
    if (!m_service->profileService()->exportProfile(path, currentEntries(), &err))
        emit toastRequested(QStringLiteral("导出失败：%1").arg(err), 3200);
    else
        emit toastRequested(QStringLiteral("方案已导出至 %1").arg(QDir::toNativeSeparators(path)),
                            2600);
}

void BackupPage::onImport()
{
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("导入关联方案"), backupDir(),
        QStringLiteral("JSON 方案 (*.json)"));
    if (path.isEmpty())
        return;

    QString err;
    const ProfileEntryList entries = m_service->profileService()->importProfile(path, &err);
    if (!err.isEmpty()) {
        emit toastRequested(QStringLiteral("导入失败：%1").arg(err), 3200);
        return;
    }
    if (entries.isEmpty()) {
        emit toastRequested(QStringLiteral("方案中没有任何关联项"), 2600);
        return;
    }

    const ProfileDiffList diffs = m_service->profileService()->diffAgainstCurrent(
        entries, m_service->reader());
    applyDiff(diffs);
}

void BackupPage::applyDiff(const ProfileDiffList &diffs)
{
    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("导入预览"));
    dialog.setModal(true);
    dialog.resize(560, 520);
    auto *layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(20, 18, 20, 16);
    layout->setSpacing(12);

    layout->addWidget(makeCaption(QStringLiteral("勾选要套用的关联项，未勾选的项将保持不变。"), &dialog));

    auto *table = new QTableWidget(diffs.size(), 3, &dialog);
    table->setHorizontalHeaderLabels(
        {QStringLiteral("目标"), QStringLiteral("当前值"), QStringLiteral("目标值")});
    table->verticalHeader()->setVisible(false);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    for (int i = 0; i < diffs.size(); ++i) {
        const ProfileDiff &d = diffs.at(i);
        auto *item0 = new QTableWidgetItem(d.target);
        item0->setFlags(item0->flags() | Qt::ItemIsUserCheckable);
        item0->setCheckState(d.selected ? Qt::Checked : Qt::Unchecked);
        table->setItem(i, 0, item0);
        table->setItem(i, 1, new QTableWidgetItem(d.currentProgId));
        table->setItem(i, 2, new QTableWidgetItem(d.targetProgId));
    }
    table->resizeColumnsToContents();
    layout->addWidget(table, 1);

    auto *buttons = new QHBoxLayout;
    buttons->addStretch(1);
    auto *cancel = new PillButton(QStringLiteral("取消"), PillButton::Secondary, &dialog);
    connect(cancel, &QPushButton::clicked, &dialog, &QDialog::reject);
    buttons->addWidget(cancel);
    auto *apply = new PillButton(QStringLiteral("应用所选"), PillButton::Primary, &dialog);
    connect(apply, &QPushButton::clicked, &dialog, &QDialog::accept);
    buttons->addWidget(apply);
    layout->addLayout(buttons);

    if (dialog.exec() != QDialog::Accepted)
        return;

    ProfileEntryList chosen;
    for (int i = 0; i < diffs.size(); ++i) {
        if (table->item(i, 0)->checkState() == Qt::Checked) {
            const ProfileDiff &d = diffs.at(i);
            chosen.append(ProfileEntry{d.target, d.targetProgId, d.isProtocol});
        }
    }
    if (chosen.isEmpty())
        return;

    const SetResultList results = m_service->rollbackTo(chosen);
    const int ok = std::count_if(results.begin(), results.end(),
                                 [](const SetResult &r) { return r.status == SetStatus::Success; });
    emit toastRequested(QStringLiteral("导入套用：成功 %1 / 共 %2").arg(ok).arg(results.size()), 3000);
    onRefreshSnapshots();
}

void BackupPage::onRefreshSnapshots()
{
    buildSnapshotList();
}

void BackupPage::buildSnapshotList()
{
    auto *layout = qobject_cast<QVBoxLayout *>(m_snapList->layout());
    Q_ASSERT(layout);
    QLayoutItem *child = nullptr;
    while ((child = layout->takeAt(0)) != nullptr) {
        delete child->widget();
        delete child;
    }

    const BackupInfoList snapshots = m_service->backupService()->listSnapshots();
    m_emptyHint->setVisible(snapshots.isEmpty());

    for (const BackupInfo &info : snapshots) {
        auto *card = new QWidget(m_snapList);
        card->setObjectName(QStringLiteral("card"));
        auto *cl = new QHBoxLayout(card);
        cl->setContentsMargins(16, 12, 16, 12);
        cl->setSpacing(12);

        auto *text = new QVBoxLayout;
        text->setSpacing(2);
        auto *time = new QLabel(info.createdAt.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")),
                                card);
        QFont tf = time->font();
        tf.setWeight(QFont::DemiBold);
        time->setFont(tf);
        text->addWidget(time);
        text->addWidget(makeCaption(
            QStringLiteral("来源：%1 · 条目：%2").arg(info.source).arg(info.entryCount), card));
        cl->addLayout(text, 1);

        auto *view = new PillButton(QStringLiteral("查看"), PillButton::Secondary, card);
        connect(view, &QPushButton::clicked, this,
                [this, fp = info.filePath] { onViewSnapshot(fp); });
        cl->addWidget(view, 0, Qt::AlignRight);

        auto *rollback = new PillButton(QStringLiteral("回滚"), PillButton::Primary, card);
        connect(rollback, &QPushButton::clicked, this,
                [this, fp = info.filePath] { onRollbackSnapshot(fp); });
        cl->addWidget(rollback, 0, Qt::AlignRight);

        layout->addWidget(card);
    }
    layout->addStretch(1);
}

void BackupPage::onViewSnapshot(const QString &filePath)
{
    QString err;
    const ProfileEntryList entries = m_service->backupService()->loadSnapshot(filePath, &err);
    if (!err.isEmpty()) {
        emit toastRequested(QStringLiteral("读取快照失败：%1").arg(err), 3200);
        return;
    }

    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("快照详情"));
    dialog.setModal(true);
    dialog.resize(420, 460);
    auto *layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(20, 18, 20, 16);
    auto *list = new QListWidget(&dialog);
    for (const ProfileEntry &e : entries)
        new QListWidgetItem(QStringLiteral("%1  →  %2").arg(e.target, e.progId), list);
    layout->addWidget(list, 1);
    auto *close = new PillButton(QStringLiteral("关闭"), PillButton::Primary, &dialog);
    connect(close, &QPushButton::clicked, &dialog, &QDialog::accept);
    layout->addWidget(close, 0, Qt::AlignRight);
    dialog.exec();
}

void BackupPage::onRollbackSnapshot(const QString &filePath)
{
    QString err;
    const ProfileEntryList entries = m_service->backupService()->loadSnapshot(filePath, &err);
    if (!err.isEmpty()) {
        emit toastRequested(QStringLiteral("读取快照失败：%1").arg(err), 3200);
        return;
    }

    if (QMessageBox::question(this, QStringLiteral("确认回滚"),
                              QStringLiteral("将用所选快照恢复 %1 项关联？").arg(entries.size()))
        != QMessageBox::Yes)
        return;

    const SetResultList results = m_service->rollbackTo(entries);
    const int ok = std::count_if(results.begin(), results.end(),
                                 [](const SetResult &r) { return r.status == SetStatus::Success; });
    emit toastRequested(QStringLiteral("回滚完成：成功 %1 / 共 %2").arg(ok).arg(results.size()), 3000);
    onRefreshSnapshots();
}

void BackupPage::onRecoverSelected()
{
    const ProfileEntryList entries = currentEntries();
    if (entries.isEmpty()) {
        emit toastRequested(QStringLiteral("尚无可操作的目标，请先等待扫描完成"), 2600);
        return;
    }

    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("选择要恢复的类型"));
    dialog.setModal(true);
    dialog.resize(360, 480);
    auto *layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(20, 18, 20, 16);
    auto *list = new QListWidget(&dialog);
    for (const ProfileEntry &e : entries) {
        auto *item = new QListWidgetItem(e.target, list);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(Qt::Unchecked);
        item->setData(Qt::UserRole, QVariant::fromValue(e.target));
        item->setData(Qt::UserRole + 1, e.isProtocol);
    }
    layout->addWidget(list, 1);

    auto *buttons = new QHBoxLayout;
    buttons->addStretch(1);
    auto *cancel = new PillButton(QStringLiteral("取消"), PillButton::Secondary, &dialog);
    connect(cancel, &QPushButton::clicked, &dialog, &QDialog::reject);
    buttons->addWidget(cancel);
    auto *okBtn = new PillButton(QStringLiteral("恢复默认"), PillButton::Danger, &dialog);
    connect(okBtn, &QPushButton::clicked, &dialog, &QDialog::accept);
    buttons->addWidget(okBtn);
    layout->addLayout(buttons);

    if (dialog.exec() != QDialog::Accepted)
        return;

    TargetRefList refs;
    for (int i = 0; i < list->count(); ++i) {
        QListWidgetItem *item = list->item(i);
        if (item->checkState() == Qt::Checked)
            refs.append({item->data(Qt::UserRole).toString(),
                         item->data(Qt::UserRole + 1).toBool()});
    }
    if (refs.isEmpty())
        return;

    const SetResultList results = m_service->restoreDefaults(refs);
    const int ok = std::count_if(results.begin(), results.end(),
                                 [](const SetResult &r) { return r.status == SetStatus::Success; });
    emit toastRequested(QStringLiteral("恢复系统默认：成功 %1 / 共 %2").arg(ok).arg(results.size()),
                        3000);
}

} // namespace das::ui
