#include "ui/dialogs/AppPickerDialog.h"

#include "core/Logging.h"
#include "platform/win/IconProvider.h"
#include "ui/ThemeManager.h"
#include "ui/UiKit.h"

#include <QFileDialog>
#include <QFileInfo>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPainter>
#include <QPushButton>
#include <QStyledItemDelegate>
#include <QVBoxLayout>

namespace das::ui {

namespace {

constexpr int kProgIdRole  = Qt::UserRole + 1;
constexpr int kAppNameRole = Qt::UserRole + 2;
constexpr int kExePathRole = Qt::UserRole + 3;
constexpr int kItemHeight  = 54;

/// 两行式候选项：图标 + 程序名 + 灰色路径。
class CandidateDelegate : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        Q_UNUSED(option)
        Q_UNUSED(index)
        return {200, kItemHeight};
    }

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override
    {
        const ThemeManager &theme = ThemeManager::instance();
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing, true);
        painter->setRenderHint(QPainter::SmoothPixmapTransform, true);

        const bool selected = option.state.testFlag(QStyle::State_Selected);
        const bool hovered  = option.state.testFlag(QStyle::State_MouseOver);

        QRect rect = option.rect.adjusted(2, 2, -2, -2);
        if (selected || hovered) {
            painter->setPen(Qt::NoPen);
            painter->setBrush(selected
                                  ? theme.color(QStringLiteral("accentSubtle"), QColor(0, 0, 0, 20))
                                  : theme.color(QStringLiteral("controlHover"), QColor(0, 0, 0, 10)));
            painter->drawRoundedRect(rect, 6, 6);
        }

        int x = rect.left() + 12;
        const QIcon icon = qvariant_cast<QIcon>(index.data(Qt::DecorationRole));
        if (!icon.isNull()) {
            icon.paint(painter, QRect(x, rect.center().y() - 14, 28, 28), Qt::AlignCenter);
        }
        x += 28 + 12;

        const int width = rect.right() - x - 10;
        if (width <= 0) {
            painter->restore();
            return;
        }

        const QString name = index.data(kAppNameRole).toString();
        const QString path = index.data(kExePathRole).toString();

        QFont nameFont = option.font;
        nameFont.setWeight(QFont::DemiBold);
        painter->setFont(nameFont);
        painter->setPen(theme.text());
        painter->drawText(QRect(x, rect.center().y() - 18, width, 19),
                          Qt::AlignVCenter | Qt::AlignLeft,
                          QFontMetrics(nameFont).elidedText(name, Qt::ElideRight, width));

        const QFont pathFont = scaledFont(option.font, -1.2);
        painter->setFont(pathFont);
        painter->setPen(theme.textTertiary());
        const QString sub = path.isEmpty() ? index.data(kProgIdRole).toString() : path;
        painter->drawText(QRect(x, rect.center().y(), width, 18), Qt::AlignVCenter | Qt::AlignLeft,
                          QFontMetrics(pathFont).elidedText(sub, Qt::ElideMiddle, width));

        painter->restore();
    }
};

QString describeTargets(const TargetRefList &targets)
{
    if (targets.isEmpty())
        return QStringLiteral("选择程序");
    if (targets.size() == 1) {
        const TargetRef &ref = targets.first();
        return ref.second ? QStringLiteral("选择处理 %1: 协议的程序").arg(ref.first)
                          : QStringLiteral("选择打开 %1 文件的程序").arg(ref.first);
    }
    return QStringLiteral("为 %1 个类型选择同一个程序").arg(targets.size());
}

} // namespace

AppPickerDialog::AppPickerDialog(AssociationService *service, const TargetRefList &targets,
                                 QWidget *parent)
    : QDialog(parent)
    , m_service(service)
    , m_targets(targets)
{
    setWindowTitle(QStringLiteral("选择默认程序"));
    setModal(true);
    resize(560, 580);
    setWindowFlag(Qt::WindowContextHelpButtonHint, false);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(22, 20, 22, 18);
    layout->setSpacing(12);

    layout->addWidget(makeSectionTitle(describeTargets(targets), this));

    QString hint = QStringLiteral("列表来自系统登记的候选程序；若没有想要的程序，可直接浏览可执行文件。");
    if (targets.size() > 1)
        hint += QStringLiteral(" 候选项已合并各类型的可用程序。");
    layout->addWidget(makeCaption(hint, this));

    m_list = new QListWidget(this);
    m_list->setItemDelegate(new CandidateDelegate(m_list));
    m_list->setIconSize(QSize(28, 28));
    m_list->setMouseTracking(true);
    m_list->setUniformItemSizes(true);
    m_list->setSelectionMode(QAbstractItemView::SingleSelection);
    m_list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    layout->addWidget(m_list, 1);

    m_pathLabel = makeCaption(QString(), this);
    m_pathLabel->setMinimumHeight(18);
    layout->addWidget(m_pathLabel);

    auto *buttons = new QHBoxLayout;
    buttons->setSpacing(10);

    auto *browse = new PillButton(QStringLiteral("浏览其他程序…"), PillButton::Secondary, this);
    connect(browse, &QPushButton::clicked, this, &AppPickerDialog::onBrowse);
    buttons->addWidget(browse);
    buttons->addStretch(1);

    auto *cancel = new PillButton(QStringLiteral("取消"), PillButton::Secondary, this);
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    buttons->addWidget(cancel);

    m_okButton = new PillButton(QStringLiteral("设为默认"), PillButton::Primary, this);
    m_okButton->setEnabled(false);
    connect(m_okButton, &QPushButton::clicked, this, &QDialog::accept);
    buttons->addWidget(m_okButton);

    layout->addLayout(buttons);

    connect(m_list, &QListWidget::itemSelectionChanged, this, &AppPickerDialog::onSelectionChanged);
    connect(m_list, &QListWidget::itemDoubleClicked, this, &AppPickerDialog::onItemActivated);

    loadCandidates();
}

void AppPickerDialog::loadCandidates()
{
    if (!m_service)
        return;

    AppCandidateList merged;
    QSet<QString>    seen;

    // 合并前若干个目标的候选，避免大批量时反复查询注册表
    const int probeCount = qMin(m_targets.size(), 6);
    for (int i = 0; i < probeCount; ++i) {
        const TargetRef &ref = m_targets.at(i);
        for (const AppCandidate &candidate : m_service->candidatesFor(ref.first, ref.second)) {
            const QString key = candidate.progId.toLower();
            if (key.isEmpty() || seen.contains(key))
                continue;
            seen.insert(key);
            merged.append(candidate);
        }
    }

    std::sort(merged.begin(), merged.end(), [](const AppCandidate &a, const AppCandidate &b) {
        if (a.isCurrent != b.isCurrent)
            return a.isCurrent;
        return a.displayName.compare(b.displayName, Qt::CaseInsensitive) < 0;
    });

    for (const AppCandidate &candidate : merged)
        appendCandidate(candidate, candidate.isCurrent && m_targets.size() == 1);

    if (merged.isEmpty()) {
        auto *item = new QListWidgetItem(QStringLiteral("没有找到候选程序"), m_list);
        item->setData(kAppNameRole, QStringLiteral("没有找到候选程序"));
        item->setData(kExePathRole, QStringLiteral("请使用「浏览其他程序…」手动指定"));
        item->setFlags(Qt::NoItemFlags);
    }
}

void AppPickerDialog::appendCandidate(const AppCandidate &candidate, bool select)
{
    auto *item = new QListWidgetItem(m_list);
    const QString name = candidate.displayName.isEmpty() ? candidate.progId : candidate.displayName;
    item->setData(Qt::DisplayRole, name);
    item->setData(kAppNameRole, candidate.isCurrent ? name + QStringLiteral("（当前默认）") : name);
    item->setData(kProgIdRole, candidate.progId);
    item->setData(kExePathRole, candidate.exePath);

    const QString iconKey = candidate.exePath.isEmpty() ? QString() : candidate.exePath;
    if (!iconKey.isEmpty())
        item->setData(Qt::DecorationRole, win::IconProvider::instance().iconForBlocking(iconKey));
    else
        item->setData(Qt::DecorationRole, win::IconProvider::instance().placeholder());

    if (select) {
        m_list->setCurrentItem(item);
        m_list->scrollToItem(item);
    }
}

void AppPickerDialog::onSelectionChanged()
{
    QListWidgetItem *item = m_list->currentItem();
    const bool valid = item && !item->data(kProgIdRole).toString().isEmpty();
    m_okButton->setEnabled(valid);
    m_pathLabel->setText(valid ? item->data(kExePathRole).toString() : QString());
}

void AppPickerDialog::onItemActivated(QListWidgetItem *item)
{
    if (item && !item->data(kProgIdRole).toString().isEmpty())
        accept();
}

void AppPickerDialog::onBrowse()
{
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("选择可执行文件"), QString(),
        QStringLiteral("可执行文件 (*.exe);;所有文件 (*.*)"));
    if (path.isEmpty())
        return;

    const TargetRef ref = m_targets.isEmpty() ? TargetRef{QString(), false} : m_targets.first();

    QString error;
    const QString progId = m_service->ensureProgIdForExe(path, ref.first, ref.second, &error);
    if (progId.isEmpty()) {
        m_pathLabel->setText(QStringLiteral("无法为该程序创建关联：%1").arg(error));
        qCWarning(logAssoc) << "为 exe 创建 ProgId 失败：" << path << error;
        return;
    }

    AppCandidate candidate;
    candidate.progId      = progId;
    candidate.displayName = QFileInfo(path).completeBaseName();
    candidate.exePath     = QDir::toNativeSeparators(path);
    appendCandidate(candidate, true);
}

void AppPickerDialog::accept()
{
    QListWidgetItem *item = m_list->currentItem();
    if (!item)
        return;
    m_progId  = item->data(kProgIdRole).toString();
    m_appName = item->data(Qt::DisplayRole).toString();
    m_exePath = item->data(kExePathRole).toString();
    if (m_progId.isEmpty())
        return;
    QDialog::accept();
}

QString AppPickerDialog::pick(AssociationService *service, const TargetRefList &targets,
                              QWidget *parent, QString *appName)
{
    AppPickerDialog dialog(service, targets, parent);
    if (dialog.exec() != QDialog::Accepted)
        return {};
    if (appName)
        *appName = dialog.selectedAppName();
    return dialog.selectedProgId();
}

} // namespace das::ui
