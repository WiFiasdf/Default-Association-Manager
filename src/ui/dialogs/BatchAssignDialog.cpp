#include "ui/dialogs/BatchAssignDialog.h"

#include "core/Logging.h"
#include "platform/win/IconProvider.h"
#include "services/AssociationService.h"
#include "ui/dialogs/AppPickerDialog.h"
#include "ui/ThemeManager.h"
#include "ui/UiKit.h"

#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPixmap>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

namespace das::ui {

namespace {

} // namespace

BatchAssignDialog::BatchAssignDialog(AssociationService *service, const QStringList &targets,
                                 bool isProtocol, QWidget *parent)
    : QDialog(parent)
    , m_targets(targets)
    , m_isProtocol(isProtocol)
    , m_service(service)
{
    setWindowTitle(QStringLiteral("批量指派程序"));
    setModal(true);
    resize(520, 560);
    setWindowFlag(Qt::WindowContextHelpButtonHint, false);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 22, 24, 20);
    layout->setSpacing(14);

    layout->addWidget(makeSectionTitle(QStringLiteral("批量指派程序"), this));
    layout->addWidget(makeCaption(
        m_isProtocol ? QStringLiteral("将为以下 %1 个协议指定同一个默认程序。")
                     : QStringLiteral("将为以下 %1 个文件类型指定同一个默认程序。").arg(targets.size()),
        this));

    // 目标清单（只读预览）
    auto *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setFrameShape(QFrame::NoFrame);
    auto *listWidget = new QListWidget(scroll);
    listWidget->setUniformItemSizes(true);
    for (const QString &target : targets)
        new QListWidgetItem(target, listWidget);
    scroll->setWidget(listWidget);
    scroll->setMaximumHeight(150);
    layout->addWidget(scroll);

    // 选中的程序卡片
    m_appCard = new QWidget(this);
    m_appCard->setObjectName(QStringLiteral("card"));
    auto *cardLayout = new QHBoxLayout(m_appCard);
    cardLayout->setContentsMargins(16, 14, 16, 14);
    cardLayout->setSpacing(14);

    m_appIcon = new QLabel(m_appCard);
    m_appIcon->setFixedSize(36, 36);
    m_appIcon->setPixmap(win::IconProvider::instance().placeholder().pixmap(36, 36));
    cardLayout->addWidget(m_appIcon);

    auto *textCol = new QVBoxLayout;
    textCol->setSpacing(2);
    m_appNameLabel = new QLabel(QStringLiteral("尚未选择程序"), m_appCard);
    QFont nameFont = m_appNameLabel->font();
    nameFont.setWeight(QFont::DemiBold);
    m_appNameLabel->setFont(nameFont);
    m_appPathLabel = makeCaption(QString(), m_appCard);
    textCol->addWidget(m_appNameLabel);
    textCol->addWidget(m_appPathLabel);
    cardLayout->addLayout(textCol, 1);

    auto *changeBtn = new PillButton(QStringLiteral("选择程序"), PillButton::Secondary, m_appCard);
    connect(changeBtn, &QPushButton::clicked, this, &BatchAssignDialog::onPickApp);
    cardLayout->addWidget(changeBtn);

    layout->addWidget(m_appCard);

    auto *browse = new PillButton(QStringLiteral("浏览任意可执行文件…"), PillButton::Secondary, this);
    connect(browse, &QPushButton::clicked, this, &BatchAssignDialog::onBrowse);
    layout->addWidget(browse, 0, Qt::AlignLeft);

    layout->addStretch(1);

    auto *buttons = new QHBoxLayout;
    buttons->setSpacing(10);
    buttons->addStretch(1);
    auto *cancel = new PillButton(QStringLiteral("取消"), PillButton::Secondary, this);
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    buttons->addWidget(cancel);
    m_startBtn = new PillButton(QStringLiteral("开始指派"), PillButton::Primary, this);
    m_startBtn->setEnabled(false);
    connect(m_startBtn, &QPushButton::clicked, this, &QDialog::accept);
    buttons->addWidget(m_startBtn);
    layout->addLayout(buttons);
}

void BatchAssignDialog::onPickApp()
{
    if (m_targets.isEmpty() || !m_service)
        return;

    TargetRefList refs;
    const int probe = qMin(m_targets.size(), 6);
    for (int i = 0; i < probe; ++i)
        refs.append({m_targets.at(i), m_isProtocol});

    AppPickerDialog dialog(m_service, refs, this);
    if (dialog.exec() != QDialog::Accepted)
        return;
    setChosen(dialog.selectedProgId(), dialog.selectedExePath(), dialog.selectedAppName());
}

void BatchAssignDialog::onBrowse()
{
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("选择可执行文件"), QString(),
        QStringLiteral("可执行文件 (*.exe);;所有文件 (*.*)"));
    if (path.isEmpty())
        return;
    setChosen(QString(), QDir::toNativeSeparators(path), QFileInfo(path).completeBaseName());
}

void BatchAssignDialog::setChosen(const QString &progId, const QString &exePath, const QString &name)
{
    m_progId  = progId;
    m_exePath = exePath;
    m_appName = name;

    if (!exePath.isEmpty()) {
        m_appIcon->setPixmap(win::IconProvider::instance().iconForBlocking(exePath).pixmap(36, 36));
    } else if (!progId.isEmpty()) {
        m_appIcon->setPixmap(win::IconProvider::instance().iconForBlocking(progId).pixmap(36, 36));
    } else {
        m_appIcon->setPixmap(win::IconProvider::instance().placeholder().pixmap(36, 36));
    }

    m_appNameLabel->setText(name.isEmpty() ? progId : name);
    m_appPathLabel->setText(exePath.isEmpty() ? progId : exePath);
    updateStartState();
}

void BatchAssignDialog::updateStartState()
{
    m_startBtn->setEnabled(!m_progId.isEmpty() || !m_exePath.isEmpty());
}

} // namespace das::ui
