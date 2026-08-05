#pragma once

#include "core/Types.h"

#include <QDialog>

namespace das {

class AssociationService;

namespace ui {

/// 结果反馈面板：按「成功 / 需手动确认 / 失败」分组，提供逐条引导与「回滚本次操作」。
class ResultReportDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ResultReportDialog(const SetResultList &results, AssociationService *service,
                                 QWidget *parent = nullptr);

signals:
    void rollbackRequested();
    void rescanRequested();

private slots:
    void onGoToSettings(const SetResult &result);

private:
    void buildGroups();

    AssociationService *m_service = nullptr;
    SetResultList      m_results;
};

} // namespace ui
} // namespace das
