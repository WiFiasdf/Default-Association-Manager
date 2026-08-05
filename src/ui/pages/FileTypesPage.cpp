#include "ui/pages/FileTypesPage.h"

#include "core/Category.h"
#include "core/Logging.h"
#include "platform/win/IconProvider.h"
#include "services/AssociationService.h"
#include "ui/ThemeManager.h"
#include "ui/UiKit.h"
#include "ui/delegates/AssociationItemDelegate.h"
#include "ui/dialogs/AppPickerDialog.h"
#include "ui/dialogs/BatchAssignDialog.h"
#include "ui/dialogs/ResultReportDialog.h"
#include "ui/models/AssociationFilterProxy.h"
#include "ui/models/AssociationTableModel.h"

#include <QEvent>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QList>
#include <QMouseEvent>
#include <QTableView>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>

namespace das::ui {

namespace {

const QList<FileCategory> kCategoryOrder = {
    FileCategory::All,      FileCategory::Image,    FileCategory::Video,
    FileCategory::Audio,    FileCategory::Document, FileCategory::Code,
    FileCategory::Archive,  FileCategory::Executable, FileCategory::Other,
};

} // namespace

FileTypesPage::FileTypesPage(AssociationService *service, QWidget *parent)
    : PageBase(parent)
    , m_service(service)
    , m_categories(kCategoryOrder)
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(24, 18, 24, 18);
    root->setSpacing(14);

    // --- 页头 -------------------------------------------------------------
    auto *head = new QHBoxLayout;
    head->setSpacing(16);
    auto *titles = new QVBoxLayout;
    titles->setSpacing(2);
    titles->addWidget(makeSectionTitle(QStringLiteral("文件类型关联"), this));
    m_countLabel = makeCaption(QStringLiteral("正在扫描本机文件关联…"), this);
    titles->addWidget(m_countLabel);
    head->addLayout(titles, 1);

    m_batchBtn = new PillButton(QStringLiteral("批量指派"), PillButton::Primary, this);
    m_batchBtn->setIcon(QIcon(QStringLiteral(":/icons/check_light.svg")));
    connect(m_batchBtn, &QPushButton::clicked, this, &FileTypesPage::enterSelectionMode);
    head->addWidget(m_batchBtn, 0, Qt::AlignRight);
    root->addLayout(head);

    // --- 筛选条 -----------------------------------------------------------
    auto *filterRow = new QHBoxLayout;
    filterRow->setSpacing(12);

    m_search = new QLineEdit(this);
    m_search->setObjectName(QStringLiteral("searchBox"));
    m_search->setPlaceholderText(QStringLiteral("搜索扩展名或程序名…"));
    m_search->setClearButtonEnabled(true);
    m_search->setMinimumWidth(240);
    m_search->setMaximumWidth(360);
    connect(m_search, &QLineEdit::textChanged, this, [this] {
        m_searchTimer->start();
    });
    filterRow->addWidget(m_search, 0, Qt::AlignLeft);

    auto *chipWrap = new QWidget(this);
    auto *chipLayout = new QHBoxLayout(chipWrap);
    chipLayout->setContentsMargins(0, 0, 0, 0);
    chipLayout->setSpacing(8);
    m_chipWrap = chipWrap;
    rebuildCategoryChips();
    filterRow->addWidget(chipWrap, 1);

    root->addLayout(filterRow);

    m_resultLabel = makeCaption(QString(), this);
    root->addWidget(m_resultLabel);

    // --- 表格 -------------------------------------------------------------
    m_model    = new AssociationTableModel(this);
    m_proxy    = new AssociationFilterProxy(this);
    m_proxy->setSourceModel(m_model);
    m_proxy->setSortCaseSensitivity(Qt::CaseInsensitive);
    m_proxy->sort(0);

    m_view = new QTableView(this);
    m_view->setModel(m_proxy);
    m_view->setObjectName(QStringLiteral("assocTable"));
    m_view->setShowGrid(false);
    m_view->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_view->setSelectionMode(QAbstractItemView::NoSelection);
    m_view->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_view->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_view->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    // QTableView 没有 setUniformRowHeights，用固定行高达到同样的滚动性能收益
    m_view->verticalHeader()->setVisible(false);
    m_view->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    m_view->verticalHeader()->setDefaultSectionSize(56);
    m_view->horizontalHeader()->setVisible(true);
    m_view->horizontalHeader()->setSectionResizeMode(AssociationTableModel::ColumnTarget,
                                                     QHeaderView::Stretch);
    m_view->horizontalHeader()->setSectionResizeMode(AssociationTableModel::ColumnApp,
                                                     QHeaderView::Stretch);
    m_view->horizontalHeader()->setSectionResizeMode(AssociationTableModel::ColumnAction,
                                                     QHeaderView::Fixed);
    m_view->setColumnWidth(AssociationTableModel::ColumnAction, 84);
    m_view->setMinimumHeight(320);

    m_delegate = new AssociationItemDelegate(this);
    m_view->setItemDelegate(m_delegate);

    auto *tableCard = new QWidget(this);
    tableCard->setObjectName(QStringLiteral("card"));
    auto *tableLayout = new QVBoxLayout(tableCard);
    tableLayout->setContentsMargins(0, 0, 0, 0);
    tableLayout->addWidget(m_view);
    root->addWidget(tableCard, 1);

    // --- 多选操作条 -------------------------------------------------------
    m_selectionBar = new QWidget(this);
    m_selectionBar->setObjectName(QStringLiteral("selectionBar"));
    m_selectionBar->setVisible(false);
    auto *barLayout = new QHBoxLayout(m_selectionBar);
    barLayout->setContentsMargins(14, 8, 14, 8);
    barLayout->setSpacing(10);
    m_selectionCountLabel = makeCaption(QStringLiteral("已选 0 项"), m_selectionBar);
    barLayout->addWidget(m_selectionCountLabel);
    barLayout->addStretch(1);

    auto *selectAll = new PillButton(QStringLiteral("全选"), PillButton::Secondary, m_selectionBar);
    connect(selectAll, &QPushButton::clicked, this, &FileTypesPage::onSelectAll);
    barLayout->addWidget(selectAll);

    auto *assign = new PillButton(QStringLiteral("指派程序"), PillButton::Primary, m_selectionBar);
    connect(assign, &QPushButton::clicked, this, &FileTypesPage::onBatchAssign);
    barLayout->addWidget(assign);

    auto *restore = new PillButton(QStringLiteral("恢复默认"), PillButton::Secondary, m_selectionBar);
    connect(restore, &QPushButton::clicked, this, &FileTypesPage::onRestoreSelected);
    barLayout->addWidget(restore);

    auto *clear = new PillButton(QStringLiteral("取消选择"), PillButton::Secondary, m_selectionBar);
    connect(clear, &QPushButton::clicked, this, &FileTypesPage::onClearSelection);
    barLayout->addWidget(clear);

    auto *finish = new PillButton(QStringLiteral("完成"), PillButton::Secondary, m_selectionBar);
    connect(finish, &QPushButton::clicked, this, &FileTypesPage::onFinishSelection);
    barLayout->addWidget(finish);

    root->addWidget(m_selectionBar);

    // --- 信号连接 ---------------------------------------------------------
    m_searchTimer = new QTimer(this);
    m_searchTimer->setInterval(200);
    m_searchTimer->setSingleShot(true);
    connect(m_searchTimer, &QTimer::timeout, this, &FileTypesPage::onSearchChanged);

    connect(m_service, &AssociationService::scanStarted, this, &FileTypesPage::onScanStarted);
    connect(m_service, &AssociationService::scanBatch, this, &FileTypesPage::onScanBatch);
    connect(m_service, &AssociationService::scanFinished, this, &FileTypesPage::onScanFinished);
    connect(m_service, &AssociationService::entryChanged, this, &FileTypesPage::onEntryChanged);

    connect(m_model, &AssociationTableModel::checkedCountChanged, this,
            &FileTypesPage::onSelectionCountChanged);
    connect(m_model, &QAbstractTableModel::rowsInserted, this, &FileTypesPage::refreshCounts);
    connect(m_model, &QAbstractTableModel::rowsRemoved, this, &FileTypesPage::refreshCounts);

    connect(m_delegate, &AssociationItemDelegate::changeRequested, this,
            &FileTypesPage::onChangeRequested);
    connect(m_delegate, &AssociationItemDelegate::checkToggled, this,
            &FileTypesPage::onCheckToggled);

    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this,
            &FileTypesPage::onThemeChanged);

    m_view->viewport()->installEventFilter(this);

    refreshCounts();
}

void FileTypesPage::onActivated()
{
    // 关联数据常驻于模型，无需重复扫描；仅刷新筛选计数。
    refreshCounts();
}

void FileTypesPage::setCompactMode(bool compact)
{
    m_compact = compact;
    m_delegate->setCompactMode(compact);
    m_search->setMaximumWidth(compact ? 200 : 360);
    m_view->viewport()->update();
}

bool FileTypesPage::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_view->viewport()) {
        if (event->type() == QEvent::MouseMove) {
            auto *me = static_cast<QMouseEvent *>(event);
            const QModelIndex idx = m_view->indexAt(me->pos());
            if (m_delegate->updateHover(idx.row(), me->pos()))
                m_view->viewport()->update();
            return false;
        }
        if (event->type() == QEvent::Leave) {
            m_delegate->clearHover();
            m_view->viewport()->update();
            return false;
        }
    } else if (event->type() == QEvent::MouseButtonPress) {
        // 类别芯片点击
        bool ok = false;
        const int catVal = watched->property("category").toInt(&ok);
        if (ok) {
            onCategoryPicked(static_cast<FileCategory>(catVal));
            return true;
        }
    }
    return PageBase::eventFilter(watched, event);
}

void FileTypesPage::onScanStarted(int /*total*/)
{
    m_model->clear();
    m_countLabel->setText(QStringLiteral("正在扫描本机文件关联…"));
}

void FileTypesPage::onScanBatch(const AssociationEntryList &batch)
{
    m_model->appendEntries(batch);
}

void FileTypesPage::onScanFinished(int total, qint64 elapsedMs, bool /*cancelled*/)
{
    refreshCounts();
    m_countLabel->setText(QStringLiteral("已扫描 %1 个文件关联 · 用时 %2 毫秒")
                              .arg(total)
                              .arg(elapsedMs));
    emit statusMessage(QStringLiteral("扫描完成，共 %1 项").arg(total));
}

void FileTypesPage::onEntryChanged(const AssociationEntry &entry)
{
    m_model->updateEntry(entry);
}

void FileTypesPage::onSearchChanged()
{
    m_proxy->setKeyword(m_search->text().trimmed());
    refreshCounts();
}

void FileTypesPage::onCategoryPicked(das::FileCategory category)
{
    if (m_activeCategory == category)
        return;
    m_activeCategory = category;
    m_proxy->setCategory(category);
    for (auto it = m_chips.begin(); it != m_chips.end(); ++it)
        styleChip(it.value(), it.key() == category);
    refreshCounts();
}

void FileTypesPage::onSelectionCountChanged(int count)
{
    m_selectionCountLabel->setText(QStringLiteral("已选 %1 项").arg(count));
}

void FileTypesPage::onSelectAll()
{
    m_model->setCheckedRows(m_proxy->visibleSourceRows(), true);
    m_view->viewport()->update();
}

void FileTypesPage::onClearSelection()
{
    m_model->clearChecked();
    m_view->viewport()->update();
}

void FileTypesPage::onFinishSelection()
{
    exitSelectionMode();
}

void FileTypesPage::onBatchAssign()
{
    if (!m_selectionMode)
        enterSelectionMode();

    const AssociationEntryList checked = m_model->checkedEntries();
    if (checked.isEmpty()) {
        emit toastRequested(QStringLiteral("请先勾选要指派的文件类型"), 2600);
        return;
    }

    QStringList targets;
    targets.reserve(checked.size());
    for (const AssociationEntry &e : checked)
        targets.append(e.target);

    BatchAssignDialog dlg(m_service, targets, false, this);
    if (dlg.exec() != QDialog::Accepted)
        return;

    ProfileDiffList diffs;
    for (const AssociationEntry &e : checked) {
        QString pid = dlg.exePath().isEmpty()
                          ? dlg.progId()
                          : m_service->ensureProgIdForExe(dlg.exePath(), e.target, e.isProtocol);
        if (pid.isEmpty())
            pid = dlg.progId();
        ProfileDiff diff;
        diff.target        = e.target;
        diff.isProtocol    = e.isProtocol;
        diff.currentProgId = e.progId;
        diff.targetProgId  = pid;
        diff.targetAppName = dlg.appName();
        diff.selected      = true;
        diffs.append(diff);
    }

    const SetResultList results = m_service->applyProfile(diffs);
    exitSelectionMode();

    ResultReportDialog report(results, m_service, this);
    report.exec();
    emit statusMessage(QStringLiteral("批量指派完成：成功 %1 / 共 %2")
                           .arg(std::count_if(results.begin(), results.end(),
                                              [](const SetResult &r) {
                                                  return r.status == SetStatus::Success;
                                              }),
                                results.size()));
}

void FileTypesPage::onRestoreSelected()
{
    const TargetRefList refs = selectedTargets();
    if (refs.isEmpty())
        return;
    const SetResultList results = m_service->restoreDefaults(refs);
    exitSelectionMode();
    const int ok = std::count_if(results.begin(), results.end(),
                                 [](const SetResult &r) { return r.status == SetStatus::Success; });
    emit toastRequested(QStringLiteral("恢复系统默认：成功 %1 / 共 %2").arg(ok).arg(results.size()),
                        2800);
}

void FileTypesPage::onChangeRequested(const QModelIndex &index)
{
    changeEntry(m_proxy->mapToSource(index));
}

void FileTypesPage::onCheckToggled(const QModelIndex &index)
{
    const QModelIndex source = m_proxy->mapToSource(index);
    m_model->toggleChecked(source.row());
    m_view->update(index);
}

void FileTypesPage::changeEntry(const QModelIndex &sourceIndex)
{
    if (!sourceIndex.isValid())
        return;
    const AssociationEntry entry = m_model->entryAt(sourceIndex.row());
    const TargetRefList targets = {{entry.target, entry.isProtocol}};

    QString appName;
    const QString progId = AppPickerDialog::pick(m_service, targets, this, &appName);
    if (progId.isEmpty())
        return;

    const SetResult result = m_service->applyOne(entry.target, entry.isProtocol, progId, this);
    if (result.status == SetStatus::Success) {
        emit toastRequested(QStringLiteral("%1 的默认程序已更新为 %2").arg(entry.target, appName),
                            2600);
    } else if (result.status == SetStatus::NeedsUserConfirm) {
        emit toastRequested(QStringLiteral("%1 需要你在系统对话框中确认一次").arg(entry.target), 3200);
    } else {
        emit toastRequested(QStringLiteral("设置失败：%1").arg(result.message), 3600);
    }

    // 主动刷新该行（服务也可能已通过 entryChanged 通知）
    AssociationEntry updated = m_service->reload(entry.target, entry.isProtocol);
    if (!updated.target.isEmpty())
        m_model->updateEntry(updated);
}

TargetRefList FileTypesPage::selectedTargets() const
{
    TargetRefList refs;
    for (const AssociationEntry &e : m_model->checkedEntries())
        refs.append({e.target, e.isProtocol});
    return refs;
}

void FileTypesPage::enterSelectionMode()
{
    m_selectionMode = true;
    m_delegate->setSelectionMode(true);
    m_selectionBar->setVisible(true);
    m_batchBtn->setText(QStringLiteral("退出多选"));
    m_view->viewport()->update();
}

void FileTypesPage::exitSelectionMode()
{
    m_selectionMode = false;
    m_delegate->setSelectionMode(false);
    m_selectionBar->setVisible(false);
    m_batchBtn->setText(QStringLiteral("批量指派"));
    m_model->clearChecked();
    m_view->viewport()->update();
}

void FileTypesPage::refreshCounts()
{
    const int total = m_model->rowCount();
    const int shown = m_proxy->rowCount();
    if (m_search->text().isEmpty() && m_activeCategory == FileCategory::All)
        m_resultLabel->setText(QStringLiteral("共 %1 项").arg(total));
    else
        m_resultLabel->setText(QStringLiteral("显示 %1 / 共 %2 项").arg(shown).arg(total));
}

void FileTypesPage::rebuildCategoryChips()
{
    auto *layout = qobject_cast<QHBoxLayout *>(m_chipWrap->layout());
    if (!layout)
        return;

    for (FileCategory cat : m_categories) {
        const QString label = (cat == FileCategory::All) ? QStringLiteral("全部")
                                                          : categoryDisplayName(cat);
        auto *chip = new QLabel(label, m_chipWrap);
        chip->setCursor(Qt::PointingHandCursor);
        const bool active = (cat == m_activeCategory);
        chip->setProperty("category", static_cast<int>(cat));
        styleChip(chip, active);
        chip->installEventFilter(this);
        layout->addWidget(chip);
        m_chips.insert(cat, chip);
    }
}

void FileTypesPage::styleChip(QLabel *chip, bool active)
{
    const ThemeManager &theme = ThemeManager::instance();
    const QColor accent = theme.accent();
    if (active) {
        chip->setStyleSheet(QStringLiteral(
            "background:%1;color:%2;border:1px solid %3;border-radius:14px;"
            "padding:5px 14px;font-weight:600;")
                                .arg(theme.color(QStringLiteral("accentSubtle"), accent).name())
                                .arg(accent.name())
                                .arg(accent.name()));
    } else {
        chip->setStyleSheet(QStringLiteral(
            "background:%1;color:%2;border:1px solid %3;border-radius:14px;padding:5px 14px;")
                                .arg(theme.color(QStringLiteral("surfaceAlt"), QColor(0, 0, 0, 8)).name())
                                .arg(theme.textSecondary().name())
                                .arg(theme.color(QStringLiteral("border"), QColor(0, 0, 0, 26)).name()));
    }
}

void FileTypesPage::onThemeChanged()
{
    for (auto it = m_chips.begin(); it != m_chips.end(); ++it)
        styleChip(it.value(), it.key() == m_activeCategory);
    m_view->viewport()->update();
}

} // namespace das::ui
