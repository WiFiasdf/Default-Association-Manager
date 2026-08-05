#include "ui/pages/AboutPage.h"

#include "core/Paths.h"
#include "services/AssociationService.h"
#include "ui/ThemeManager.h"
#include "ui/UiKit.h"

#include <QDesktopServices>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPixmap>
#include <QPushButton>
#include <QUrl>
#include <QVBoxLayout>

namespace das::ui {

namespace {
const char kAppVersion[] = "1.0.0";
} // namespace

AboutPage::AboutPage(AssociationService *service, QWidget *parent)
    : PageBase(parent)
    , m_service(service)
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(24, 18, 24, 18);
    root->setSpacing(16);

    // 品牌区
    auto *brand = new QWidget(this);
    brand->setObjectName(QStringLiteral("card"));
    auto *brandLayout = new QHBoxLayout(brand);
    brandLayout->setContentsMargins(18, 18, 18, 18);
    brandLayout->setSpacing(16);

    auto *icon = new QLabel(brand);
    icon->setFixedSize(56, 56);
    icon->setPixmap(QIcon(QStringLiteral(":/icons/app.svg")).pixmap(56, 56));
    brandLayout->addWidget(icon);

    auto *brandText = new QVBoxLayout;
    brandText->setSpacing(3);
    auto *name = new QLabel(QStringLiteral("默认软件设置器"), brand);
    QFont nf = name->font();
    nf.setPointSize(20);
    nf.setWeight(QFont::DemiBold);
    name->setFont(nf);
    brandText->addWidget(name);
    brandText->addWidget(makeCaption(QStringLiteral("版本 %1 · Windows 11 文件 / 协议默认程序管理工具")
                                         .arg(QString::fromLatin1(kAppVersion)),
                                     brand));
    brandLayout->addLayout(brandText, 1);
    root->addWidget(brand);

    // 原理说明
    auto *principle = new QGroupBox(QStringLiteral("工作原理"), this);
    principle->setObjectName(QStringLiteral("group"));
    auto *pl = new QVBoxLayout(principle);
    pl->setSpacing(6);
    pl->addWidget(makeCaption(
        QStringLiteral("采用「混合设置策略」：优先通过 UserChoice 哈希算法静默写入本用户（HKCU）"
                       "关联并立即生效，无需手动确认；当系统校验未通过时，自动回退到系统「打开方式」"
                       "对话框或系统设置深链，引导你点击一次完成。每次写入前都会自动生成快照，可随时回滚。"),
        principle));
    root->addWidget(principle);

    // 风险提示
    auto *risk = new QWidget(this);
    risk->setObjectName(QStringLiteral("cardWarn"));
    auto *riskLayout = new QVBoxLayout(risk);
    riskLayout->setContentsMargins(16, 14, 16, 14);
    riskLayout->setSpacing(4);
    auto *riskTitle = new QLabel(QStringLiteral("风险提示"), risk);
    QFont rt = riskTitle->font();
    rt.setWeight(QFont::DemiBold);
    riskTitle->setFont(rt);
    riskLayout->addWidget(riskTitle);
    riskLayout->addWidget(makeCaption(
        QStringLiteral("静默写入为社区逆向的非官方手段，未来 Windows 更新可能改变其行为。本工具仅写入"
                       "当前用户作用域（HKCU），不修改系统文件、不请求管理员权限。请谨慎使用，后果自负。"),
        risk));
    root->addWidget(risk);

    // 链接区
    auto *links = new QWidget(this);
    auto *linkLayout = new QHBoxLayout(links);
    linkLayout->setContentsMargins(0, 0, 0, 0);
    linkLayout->setSpacing(10);
    auto *logBtn = new PillButton(QStringLiteral("打开日志目录"), PillButton::Secondary, links);
    connect(logBtn, &QPushButton::clicked, this, &AboutPage::onOpenLogDir);
    linkLayout->addWidget(logBtn);
    auto *backupBtn = new PillButton(QStringLiteral("打开备份目录"), PillButton::Secondary, links);
    connect(backupBtn, &QPushButton::clicked, this, &AboutPage::onOpenBackupDir);
    linkLayout->addWidget(backupBtn);
    linkLayout->addStretch(1);
    root->addWidget(links);

    root->addStretch(1);
}

void AboutPage::onActivated()
{
}

void AboutPage::setCompactMode(bool /*compact*/)
{
}

void AboutPage::onOpenLogDir()
{
    QDesktopServices::openUrl(QUrl::fromLocalFile(logDir()));
}

void AboutPage::onOpenBackupDir()
{
    QDesktopServices::openUrl(QUrl::fromLocalFile(backupDir()));
}

} // namespace das::ui
