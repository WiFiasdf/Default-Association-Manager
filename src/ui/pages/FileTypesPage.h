#pragma once

#include "core/Types.h"
#include "ui/PageBase.h"

#include <QHash>
#include <QSet>

class QLabel;
class QLineEdit;
class QTableView;
class QTimer;
class QWidget;

namespace das {

class AssociationService;

namespace ui {

class AssociationFilterProxy;
class AssociationTableModel;
class AssociationItemDelegate;
class BatchAssignDialog;
class PillButton;

class FileTypesPage : public PageBase
{
    Q_OBJECT

public:
    explicit FileTypesPage(AssociationService *service, QWidget *parent = nullptr);

    void onActivated() override;
    void setCompactMode(bool compact) override;

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    void onScanStarted(int total);
    void onScanBatch(const AssociationEntryList &batch);
    void onScanFinished(int total, qint64 elapsedMs, bool cancelled);
    void onEntryChanged(const AssociationEntry &entry);
    void onSearchChanged();
    void onCategoryPicked(FileCategory category);
    void onSelectionCountChanged(int count);
    void onBatchAssign();
    void onRestoreSelected();
    void onSelectAll();
    void onClearSelection();
    void onFinishSelection();
    void onChangeRequested(const QModelIndex &index);
    void onCheckToggled(const QModelIndex &index);
    void onThemeChanged();

private:
    TargetRefList selectedTargets() const;

    void changeEntry(const QModelIndex &sourceIndex);
    void refreshCounts();
    void rebuildCategoryChips();
    void styleChip(QLabel *chip, bool active);
    void enterSelectionMode();
    void exitSelectionMode();

    AssociationService      *m_service   = nullptr;
    AssociationTableModel   *m_model     = nullptr;
    AssociationFilterProxy  *m_proxy     = nullptr;
    AssociationItemDelegate *m_delegate  = nullptr;
    QTableView              *m_view      = nullptr;
    QLineEdit               *m_search    = nullptr;

    QList<FileCategory>      m_categories;
    QHash<FileCategory, QLabel *> m_chips;
    QWidget                 *m_chipWrap = nullptr;
    FileCategory             m_activeCategory = FileCategory::All;

    QLabel      *m_countLabel   = nullptr;
    QLabel      *m_resultLabel  = nullptr;
    QWidget     *m_selectionBar = nullptr;
    QLabel      *m_selectionCountLabel = nullptr;
    PillButton  *m_batchBtn     = nullptr;

    QTimer *m_searchTimer = nullptr;
    bool    m_compact     = false;
    bool    m_selectionMode = false;
};

} // namespace ui
} // namespace das
