#pragma once

#include "ui/PageBase.h"

namespace das {

class AssociationService;

namespace ui {

class PillButton;

class AboutPage : public PageBase
{
    Q_OBJECT

public:
    explicit AboutPage(AssociationService *service, QWidget *parent = nullptr);

    void onActivated() override;
    void setCompactMode(bool compact) override;

private slots:
    void onOpenLogDir();
    void onOpenBackupDir();

private:
    AssociationService *m_service = nullptr;
    bool                m_compact = false;
};

} // namespace ui
} // namespace das
