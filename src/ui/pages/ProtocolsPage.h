#pragma once

#include "core/Types.h"
#include "ui/PageBase.h"

#include <QList>
#include <QString>
#include <QStringList>

class QLabel;
class QWidget;

namespace das {

class AssociationService;

namespace ui {

class PillButton;

/// 一个协议分组在界面上对应的一行卡片。
struct ProtoRow {
    QString      display;
    QStringList  protocols;
    QLabel      *icon    = nullptr;
    QLabel      *appName = nullptr;
    QLabel      *appPath = nullptr;
    QWidget     *card    = nullptr;
};

/// 协议关联页：默认浏览器 / 邮件等分组，复用同一套设置与回退逻辑。
class ProtocolsPage : public PageBase
{
    Q_OBJECT

public:
    explicit ProtocolsPage(AssociationService *service, QWidget *parent = nullptr);

    void onActivated() override;
    void setCompactMode(bool compact) override;

private slots:
    void onProtocolChanged(int row);

private:
    void refresh();
    ProtoRow buildRow(const QString &display, const QStringList &protocols);

    AssociationService     *m_service = nullptr;
    QList<ProtoRow>         m_rows;
    QLabel                 *m_hint = nullptr;
    bool                    m_compact = false;
};

} // namespace ui
} // namespace das
