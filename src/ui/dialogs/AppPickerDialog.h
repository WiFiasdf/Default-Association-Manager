#pragma once

#include "core/Types.h"
#include "services/AssociationService.h"

#include <QDialog>
#include <QString>

class QLabel;
class QListWidget;
class QListWidgetItem;

namespace das::ui {

/// 应用选择器：列出候选程序，并允许浏览任意 exe（必要时自动注册 ProgId）。
class AppPickerDialog : public QDialog
{
    Q_OBJECT

public:
    AppPickerDialog(AssociationService *service, const TargetRefList &targets,
                    QWidget *parent = nullptr);

    QString selectedProgId() const { return m_progId; }
    QString selectedAppName() const { return m_appName; }
    /// 选中候选的可执行路径；浏览 exe 时即所选路径，系统候选可能为空。
    QString selectedExePath() const { return m_exePath; }

    /// 便捷入口：返回选中的 ProgId，取消时返回空串。
    static QString pick(AssociationService *service, const TargetRefList &targets, QWidget *parent,
                        QString *appName = nullptr);

private slots:
    void onBrowse();
    void onItemActivated(QListWidgetItem *item);
    void onSelectionChanged();

private:
    void loadCandidates();
    void appendCandidate(const AppCandidate &candidate, bool select = false);
    void accept() override;

    AssociationService *m_service = nullptr;
    TargetRefList       m_targets;

    QListWidget *m_list       = nullptr;
    QLabel      *m_pathLabel  = nullptr;
    QPushButton *m_okButton   = nullptr;

    QString m_progId;
    QString m_appName;
    QString m_exePath;
};

} // namespace das::ui
