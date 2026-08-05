#pragma once

#include "core/Types.h"

#include <QDialog>
#include <QStringList>

namespace das {

class AssociationService;

namespace ui {

/// 批量指派对话框：展示待处理的目标清单，选择同一个程序后由调用方执行批量写入。
class BatchAssignDialog : public QDialog
{
    Q_OBJECT

public:
    explicit BatchAssignDialog(AssociationService *service, const QStringList &targets,
                               bool isProtocol, QWidget *parent = nullptr);

    /// 选中的程序；exePath 非空时调用方应按目标逐个派生子 ProgId。
    QString progId() const { return m_progId; }
    QString exePath() const { return m_exePath; }
    QString appName() const { return m_appName; }

private slots:
    void onPickApp();
    void onBrowse();
    void updateStartState();

private:
    void setChosen(const QString &progId, const QString &exePath, const QString &name);

    QStringList m_targets;
    bool        m_isProtocol = false;
    AssociationService *m_service = nullptr;

    QString m_progId;
    QString m_exePath;
    QString m_appName;

    QLabel      *m_appIcon = nullptr;
    QLabel      *m_appNameLabel = nullptr;
    QLabel      *m_appPathLabel = nullptr;
    QWidget     *m_appCard = nullptr;
    QPushButton *m_startBtn = nullptr;
};

} // namespace ui
} // namespace das
