#include "ui/pages/ProtocolsPage.h"

#include "core/Logging.h"
#include "platform/win/IconProvider.h"
#include "services/AssociationService.h"
#include "ui/ThemeManager.h"
#include "ui/UiKit.h"
#include "ui/dialogs/AppPickerDialog.h"

#include <QDesktopServices>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace das::ui {

namespace {
const QList<QPair<QString, QStringList>> kGroups = {
    {QStringLiteral("默认浏览器"), {"http", "https"}},
    {QStringLiteral("默认邮件客户端"), {"mailto"}},
    {QStringLiteral("FTP"), {"ftp"}},
    {QStringLiteral("电话"), {"tel"}},
    {QStringLiteral("呼叫"), {"callto"}},
    {QStringLiteral("日历"), {"webcal"}},
    {QStringLiteral("流媒体"), {"mms"}},
};
} // namespace

ProtocolsPage::ProtocolsPage(AssociationService *service, QWidget *parent)
    : PageBase(parent)
    , m_service(service)
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(24, 18, 24, 18);
    root->setSpacing(14);

    auto *titles = new QVBoxLayout;
    titles->setSpacing(2);
    titles->addWidget(makeSectionTitle(QStringLiteral("协议与默认应用"), this));
    titles->addWidget(
        makeCaption(QStringLiteral("管理 http/https/mailto/ftp/tel 等协议的默认程序。"), this));
    root->addLayout(titles);

    m_hint = makeCaption(QStringLiteral("提示：浏览器类协议在部分系统版本需通过系统设置确认一次才能生效。"),
                         this);
    m_hint->setObjectName(QStringLiteral("hint"));
    root->addWidget(m_hint);

    for (const auto &g : kGroups)
        m_rows.append(buildRow(g.first, g.second));

    auto *list = new QWidget(this);
    auto *listLayout = new QVBoxLayout(list);
    listLayout->setContentsMargins(0, 0, 0, 0);
    listLayout->setSpacing(10);
    for (const ProtoRow &row : m_rows)
        listLayout->addWidget(row.card);
    listLayout->addStretch(1);
    root->addWidget(list, 1);

    refresh();
}

void ProtocolsPage::onActivated()
{
    refresh();
}

void ProtocolsPage::setCompactMode(bool compact)
{
    m_compact = compact;
    for (const ProtoRow &row : m_rows)
        row.card->setVisible(true);
}

ProtoRow ProtocolsPage::buildRow(const QString &display, const QStringList &protocols)
{
    ProtoRow row;
    row.display   = display;
    row.protocols = protocols;

    row.card = new QWidget(this);
    row.card->setObjectName(QStringLiteral("card"));
    auto *layout = new QHBoxLayout(row.card);
    layout->setContentsMargins(16, 14, 16, 14);
    layout->setSpacing(14);

    row.icon = new QLabel(row.card);
    row.icon->setFixedSize(40, 40);
    row.icon->setPixmap(QIcon(QStringLiteral(":/icons/app.svg")).pixmap(40, 40));
    layout->addWidget(row.icon);

    auto *text = new QVBoxLayout;
    text->setSpacing(2);
    auto *name = new QLabel(display, row.card);
    QFont nf = name->font();
    nf.setWeight(QFont::DemiBold);
    name->setFont(nf);
    QString protoText = QStringLiteral("协议：") + protocols.join(QLatin1String(", "));
    text->addWidget(name);
    text->addWidget(makeCaption(protoText, row.card));

    auto *appCol = new QVBoxLayout;
    appCol->setSpacing(1);
    row.appName = new QLabel(QStringLiteral("—"), row.card);
    row.appPath = makeCaption(QString(), row.card);
    appCol->addWidget(row.appName);
    appCol->addWidget(row.appPath);
    layout->addLayout(text, 1);
    layout->addLayout(appCol, 1);

    auto *change = new PillButton(QStringLiteral("更改"), PillButton::Secondary, row.card);
    const int index = m_rows.size();
    connect(change, &QPushButton::clicked, this, [this, index] { onProtocolChanged(index); });
    layout->addWidget(change, 0, Qt::AlignRight);

    return row;
}

void ProtocolsPage::refresh()
{
    for (ProtoRow &row : m_rows) {
        const AssociationEntry entry = m_service->reload(row.protocols.first(), true);
        if (entry.appName.isEmpty()) {
            row.appName->setText(QStringLiteral("未设置"));
            row.appPath->setText(QString());
        } else {
            row.appName->setText(entry.appName);
            row.appPath->setText(entry.appPath);
        }
        if (!entry.appPath.isEmpty()) {
            row.icon->setPixmap(
                win::IconProvider::instance().iconForBlocking(entry.appPath).pixmap(40, 40));
        } else if (!entry.progId.isEmpty()) {
            row.icon->setPixmap(
                win::IconProvider::instance().iconForBlocking(entry.progId).pixmap(40, 40));
        }
    }
}

void ProtocolsPage::onProtocolChanged(int row)
{
    if (row < 0 || row >= m_rows.size())
        return;
    ProtoRow &r = m_rows[row];

    const TargetRefList targets = {{r.protocols.first(), true}};
    QString appName;
    const QString progId = AppPickerDialog::pick(m_service, targets, this, &appName);
    if (progId.isEmpty())
        return;

    int ok = 0;
    for (const QString &proto : r.protocols) {
        const SetResult result = m_service->applyOne(proto, true, progId, this);
        if (result.status == SetStatus::Success)
            ++ok;
    }
    emit toastRequested(QStringLiteral("%1 已更新为 %2（%3/%4）")
                            .arg(r.display)
                            .arg(appName)
                            .arg(ok)
                            .arg(r.protocols.size()),
                        3000);
    refresh();
}

} // namespace das::ui
