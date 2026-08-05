#include "ui/dialogs/ResultReportDialog.h"

#include "services/AssociationService.h"
#include "ui/ThemeManager.h"
#include "ui/UiKit.h"

#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

namespace das::ui {

namespace {

QColor statusColor(das::SetStatus status)
{
    switch (status) {
    case das::SetStatus::Success:
        return ThemeManager::instance().color(QStringLiteral("ok"), QColor(0x0F, 0x7B, 0x0F));
    case das::SetStatus::NeedsUserConfirm:
        return ThemeManager::instance().color(QStringLiteral("warn"), QColor(0x9D, 0x5D, 0x00));
    case das::SetStatus::Failed:
        return ThemeManager::instance().color(QStringLiteral("error"), QColor(0xC4, 0x2B, 0x1C));
    case das::SetStatus::Skipped:
        break;
    }
    return ThemeManager::instance().textSecondary();
}

QString statusLabel(das::SetStatus status)
{
    switch (status) {
    case das::SetStatus::Success:
        return QStringLiteral("已静默生效");
    case das::SetStatus::NeedsUserConfirm:
        return QStringLiteral("需手动确认");
    case das::SetStatus::Failed:
        return QStringLiteral("失败");
    case das::SetStatus::Skipped:
        return QStringLiteral("已跳过");
    }
    return QStringLiteral("未知");
}

} // namespace

ResultReportDialog::ResultReportDialog(const SetResultList &results, AssociationService *service,
                                 QWidget *parent)
    : QDialog(parent)
    , m_service(service)
    , m_results(results)
{
    setWindowTitle(QStringLiteral("设置结果"));
    setModal(true);
    resize(540, 600);
    setWindowFlag(Qt::WindowContextHelpButtonHint, false);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(22, 20, 22, 18);
    layout->setSpacing(12);

    int ok = 0, need = 0, fail = 0;
    for (const SetResult &r : results) {
        if (r.status == SetStatus::Success)
            ++ok;
        else if (r.status == SetStatus::NeedsUserConfirm)
            ++need;
        else
            ++fail;
    }

    layout->addWidget(makeSectionTitle(QStringLiteral("设置结果"), this));
    layout->addWidget(makeCaption(
        QStringLiteral("成功 %1 项 · 需手动确认 %2 项 · 失败 %3 项。").arg(ok).arg(need).arg(fail),
        this));

    auto *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setFrameShape(QFrame::NoFrame);
    auto *body = new QWidget(scroll);
    auto *bodyLayout = new QVBoxLayout(body);
    bodyLayout->setContentsMargins(0, 0, 0, 0);
    bodyLayout->setSpacing(10);
    scroll->setWidget(body);
    layout->addWidget(scroll, 1);

    auto group = [&](const QString &title, das::SetStatus status, bool expanded, bool danger) {
        QList<SetResult> items;
        for (const SetResult &r : m_results)
            if (r.status == status)
                items.append(r);
        if (items.isEmpty())
            return;

        auto *box = new QGroupBox(QStringLiteral("%1（%2）").arg(title).arg(items.size()), body);
        box->setObjectName(danger ? QStringLiteral("groupDanger") : QStringLiteral("group"));
        auto *bl = new QVBoxLayout(box);
        bl->setSpacing(8);
        // QGroupBox 自身不提供折叠能力，这里借助 checkable 状态手动控制子项显隐
        box->setCheckable(true);
        for (const SetResult &r : items) {
            auto *row = new QWidget(box);
            auto *rl = new QHBoxLayout(row);
            rl->setContentsMargins(0, 0, 0, 0);
            rl->setSpacing(10);

            auto *dot = new QLabel(row);
            dot->setFixedSize(10, 10);
            dot->setStyleSheet(QStringLiteral("background:%1;border-radius:5px;").arg(
                statusColor(status).name()));
            rl->addWidget(dot);

            auto *text = new QVBoxLayout;
            text->setSpacing(1);
            auto *name = new QLabel(r.target, row);
            QFont nf = name->font();
            nf.setWeight(QFont::DemiBold);
            name->setFont(nf);
            auto *msg = makeCaption(r.message.isEmpty() ? statusLabel(status) : r.message, row);
            text->addWidget(name);
            text->addWidget(msg);
            rl->addLayout(text, 1);

            if (status == SetStatus::NeedsUserConfirm) {
                auto *go = new PillButton(QStringLiteral("去设置"), PillButton::Secondary, row);
                connect(go, &QPushButton::clicked, this,
                        [this, r] { onGoToSettings(r); });
                rl->addWidget(go);
            }
            bl->addWidget(row);
        }
        const QList<QWidget *> rows = box->findChildren<QWidget *>(QString(), Qt::FindDirectChildrenOnly);
        auto setRowsVisible = [rows](bool visible) {
            for (QWidget *w : rows)
                w->setVisible(visible);
        };
        connect(box, &QGroupBox::toggled, box, setRowsVisible);
        box->setChecked(expanded);
        setRowsVisible(expanded);
        bodyLayout->addWidget(box);
    };

    group(QStringLiteral("需手动确认"), SetStatus::NeedsUserConfirm, true, true);
    group(QStringLiteral("失败"), SetStatus::Failed, false, true);
    group(QStringLiteral("成功"), SetStatus::Success, false, false);
    bodyLayout->addStretch(1);

    auto *buttons = new QHBoxLayout;
    buttons->setSpacing(10);
    auto *rollback = new PillButton(QStringLiteral("回滚本次操作"), PillButton::Danger, this);
    connect(rollback, &QPushButton::clicked, this, [this] {
        emit rollbackRequested();
        accept();
    });
    buttons->addWidget(rollback);
    buttons->addStretch(1);
    auto *done = new PillButton(QStringLiteral("完成"), PillButton::Primary, this);
    connect(done, &QPushButton::clicked, this, &QDialog::accept);
    buttons->addWidget(done);
    layout->addLayout(buttons);
}

void ResultReportDialog::onGoToSettings(const das::SetResult &result)
{
    if (!m_service)
        return;
    const SetResult updated = m_service->runGuided(result, this);
    // 更新记录中的对应项，便于再次查看时反映最新状态
    for (SetResult &r : m_results) {
        if (r.target == result.target && r.progId == result.progId) {
            r.status = updated.status;
            r.message = updated.message;
            break;
        }
    }
}

} // namespace das::ui
