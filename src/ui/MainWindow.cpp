#include "ui/MainWindow.h"

#include "core/Logging.h"
#include "platform/win/WindowEffects.h"
#include "platform/win/WinUtils.h"
#include "services/AssociationService.h"
#include "ui/NavigationRail.h"
#include "ui/PageBase.h"
#include "ui/ThemeManager.h"
#include "ui/Toast.h"
#include "ui/UiKit.h"
#include "ui/pages/AboutPage.h"
#include "ui/pages/BackupPage.h"
#include "ui/pages/FileTypesPage.h"
#include "ui/pages/ProtocolsPage.h"

#include <QCloseEvent>
#include <QGraphicsOpacityEffect>
#include <QHBoxLayout>
#include <QLabel>
#include <QParallelAnimationGroup>
#include <QProgressBar>
#include <QPropertyAnimation>
#include <QShowEvent>
#include <QStackedWidget>
#include <QTimer>
#include <QVBoxLayout>

namespace das {

namespace {

constexpr int kRailCollapseWidth = 1000; ///< 窄于此宽度自动折叠导航栏
constexpr int kCompactWidth      = 880;  ///< 窄于此宽度进入紧凑模式

} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("rootFrame"));
    setAttribute(Qt::WA_StyledBackground, true);
    setWindowTitle(QStringLiteral("默认软件设置器"));
    setMinimumSize(900, 600);
    resize(1120, 720);

    m_service = new AssociationService(this);

    buildUi();
    connectService();

    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this,
            &MainWindow::onThemeChanged);
}

MainWindow::~MainWindow() = default;

// ---------------------------------------------------------------------------
// 构建
// ---------------------------------------------------------------------------

void MainWindow::buildUi()
{
    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    buildTopBar(rootLayout);
    buildBody(rootLayout);
    buildStatusBar(rootLayout);
}

void MainWindow::buildTopBar(QVBoxLayout *rootLayout)
{
    auto *topBar = new QWidget(this);
    topBar->setObjectName(QStringLiteral("topBar"));
    topBar->setFixedHeight(52);

    auto *layout = new QHBoxLayout(topBar);
    layout->setContentsMargins(14, 8, 14, 8);
    layout->setSpacing(10);

    m_menuButton = new ui::IconButton(QStringLiteral(":/icons/menu.svg"),
                                      QStringLiteral("折叠 / 展开导航栏"), 18, topBar);
    connect(m_menuButton, &QToolButton::clicked, this, [this]() {
        m_railManuallyCollapsed = !m_rail->isCollapsed();
        m_rail->setCollapsed(m_railManuallyCollapsed);
    });
    layout->addWidget(m_menuButton);

    auto *iconLabel = new QLabel(topBar);
    iconLabel->setFixedSize(22, 22);
    iconLabel->setPixmap(ui::renderSvg(QStringLiteral(":/icons/app.svg"), 22, QColor(),
                                       devicePixelRatioF()));
    layout->addWidget(iconLabel);

    auto *title = new QLabel(QStringLiteral("默认软件设置器"), topBar);
    title->setObjectName(QStringLiteral("appTitle"));
    layout->addWidget(title);

    auto *version = new QLabel(QStringLiteral("v%1").arg(QStringLiteral(DAS_VERSION_STRING)), topBar);
    version->setObjectName(QStringLiteral("appVersion"));
    layout->addWidget(version);

    layout->addStretch(1);

    m_refreshButton = new ui::IconButton(QStringLiteral(":/icons/refresh.svg"),
                                         QStringLiteral("重新扫描关联（F5）"), 18, topBar);
    connect(m_refreshButton, &QToolButton::clicked, this, [this]() {
        if (m_service->isScanning())
            return;
        m_service->startScan();
    });
    layout->addWidget(m_refreshButton);

    m_themeButton = new ui::IconButton(QStringLiteral(":/icons/theme.svg"),
                                       QStringLiteral("切换浅色 / 深色主题"), 18, topBar);
    connect(m_themeButton, &QToolButton::clicked, this,
            []() { ThemeManager::instance().toggle(); });
    layout->addWidget(m_themeButton);

    rootLayout->addWidget(topBar);
}

void MainWindow::buildBody(QVBoxLayout *rootLayout)
{
    auto *body = new QWidget(this);
    auto *layout = new QHBoxLayout(body);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_rail = new ui::NavigationRail(body);
    layout->addWidget(m_rail);

    auto *pane = new QFrame(body);
    pane->setObjectName(QStringLiteral("contentPane"));
    pane->setAttribute(Qt::WA_StyledBackground, true);
    auto *paneLayout = new QVBoxLayout(pane);
    paneLayout->setContentsMargins(0, 0, 0, 0);
    paneLayout->setSpacing(0);

    m_stack = new QStackedWidget(pane);
    m_stack->setObjectName(QStringLiteral("contentStack"));
    paneLayout->addWidget(m_stack);

    layout->addWidget(pane, 1);
    rootLayout->addWidget(body, 1);

    m_fileTypesPage = new ui::FileTypesPage(m_service, m_stack);
    m_protocolsPage = new ui::ProtocolsPage(m_service, m_stack);
    m_backupPage    = new ui::BackupPage(m_service, m_stack);
    m_aboutPage     = new ui::AboutPage(m_service, m_stack);

    registerPage(m_fileTypesPage, QStringLiteral(":/icons/nav_filetypes.svg"),
                 QStringLiteral("文件类型"), QStringLiteral("按扩展名查看并修改默认打开方式"));
    registerPage(m_protocolsPage, QStringLiteral(":/icons/nav_protocols.svg"),
                 QStringLiteral("协议关联"), QStringLiteral("默认浏览器、邮件客户端等协议处理程序"));
    registerPage(m_backupPage, QStringLiteral(":/icons/nav_backup.svg"),
                 QStringLiteral("备份还原"), QStringLiteral("快照回滚、恢复系统默认与方案导入导出"));
    registerPage(m_aboutPage, QStringLiteral(":/icons/nav_about.svg"),
                 QStringLiteral("关于"), QStringLiteral("版本、实现原理与风险说明"));

    connect(m_rail, &ui::NavigationRail::currentChanged, this, &MainWindow::onNavigationChanged);
    m_stack->setCurrentIndex(0);
}

void MainWindow::buildStatusBar(QVBoxLayout *rootLayout)
{
    auto *bar = new QWidget(this);
    bar->setObjectName(QStringLiteral("statusBar"));
    bar->setFixedHeight(34);

    auto *layout = new QHBoxLayout(bar);
    layout->setContentsMargins(18, 0, 18, 0);
    layout->setSpacing(12);

    m_statusText = new QLabel(QStringLiteral("准备就绪"), bar);
    m_statusText->setObjectName(QStringLiteral("statusText"));
    layout->addWidget(m_statusText);

    layout->addStretch(1);

    m_progress = new QProgressBar(bar);
    m_progress->setFixedSize(180, 4);
    m_progress->setTextVisible(false);
    m_progress->setRange(0, 0);
    m_progress->hide();
    layout->addWidget(m_progress);

    const unsigned build = win::realBuildNumber();
    m_statusHint = new QLabel(bar);
    m_statusHint->setObjectName(QStringLiteral("statusHint"));
    m_statusHint->setText(QStringLiteral("Windows Build %1 · 仅写入当前用户配置（HKCU），无需管理员权限")
                              .arg(build));
    layout->addWidget(m_statusHint);

    rootLayout->addWidget(bar);
}

void MainWindow::registerPage(ui::PageBase *page, const QString &iconPath, const QString &title,
                              const QString &description)
{
    m_stack->addWidget(page);
    m_rail->addItem(iconPath, title, description);

    connect(page, &ui::PageBase::statusMessage, this, [this](const QString &text) {
        m_statusText->setText(text);
    });
    connect(page, &ui::PageBase::toastRequested, this, &MainWindow::showToast);
    connect(page, &ui::PageBase::busyChanged, this, &MainWindow::setBusy);
}

void MainWindow::connectService()
{
    connect(m_service, &AssociationService::scanStarted, this, &MainWindow::onScanStarted);
    connect(m_service, &AssociationService::scanFinished, this, &MainWindow::onScanFinished);
}

// ---------------------------------------------------------------------------
// 事件
// ---------------------------------------------------------------------------

void MainWindow::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    if (!m_firstShow)
        return;
    m_firstShow = false;

    applyWindowEffects();
    updateResponsiveLayout();

    // 让首帧先绘制出来，再启动全量扫描，避免启动时白屏
    QTimer::singleShot(60, this, [this]() {
        m_service->startScan();
        if (m_fileTypesPage)
            m_fileTypesPage->onActivated();
    });
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    updateResponsiveLayout();
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    m_service->cancelScan();
    QWidget::closeEvent(event);
}

void MainWindow::updateResponsiveLayout()
{
    if (!m_rail)
        return;
    const bool shouldCollapse = m_railManuallyCollapsed || width() < kRailCollapseWidth;
    if (shouldCollapse != m_rail->isCollapsed())
        m_rail->setCollapsed(shouldCollapse);

    const bool compact = width() < kCompactWidth;
    for (ui::PageBase *page : {static_cast<ui::PageBase *>(m_fileTypesPage),
                               static_cast<ui::PageBase *>(m_protocolsPage),
                               static_cast<ui::PageBase *>(m_backupPage)}) {
        if (page)
            page->setCompactMode(compact);
    }

    if (m_statusHint)
        m_statusHint->setVisible(width() >= kCompactWidth);
}

void MainWindow::applyWindowEffects()
{
    const ThemeManager &theme = ThemeManager::instance();
    const bool dark = theme.isDark();

    win::effects::applyRoundedCorners(this);
    win::effects::applyDarkTitleBar(this, dark);
    win::effects::applyCaptionColors(this, theme.color(QStringLiteral("bgTop")),
                                     theme.color(QStringLiteral("textSecondary")),
                                     theme.color(QStringLiteral("border")));
}

// ---------------------------------------------------------------------------
// 槽
// ---------------------------------------------------------------------------

void MainWindow::onNavigationChanged(int index)
{
    if (!m_stack || index < 0 || index >= m_stack->count())
        return;
    m_stack->setCurrentIndex(index);

    QWidget *page = m_stack->currentWidget();
    animatePage(page);
    if (auto *base = qobject_cast<ui::PageBase *>(page))
        base->onActivated();
}

void MainWindow::onThemeChanged(bool dark)
{
    ui::clearIconCache();
    applyWindowEffects();
    update();
    qCDebug(logUi) << "主题切换：" << (dark ? "深色" : "浅色");
}

void MainWindow::onScanStarted(int total)
{
    Q_UNUSED(total)
    m_statusText->setText(QStringLiteral("正在扫描系统文件关联…"));
    m_progress->setRange(0, 0);
    m_progress->show();
    m_refreshButton->setEnabled(false);
}

void MainWindow::onScanFinished(int total, qint64 elapsedMs, bool cancelled)
{
    m_progress->hide();
    m_refreshButton->setEnabled(true);
    if (cancelled) {
        m_statusText->setText(QStringLiteral("扫描已取消"));
        return;
    }
    m_statusText->setText(
        QStringLiteral("已加载 %1 项关联 · 耗时 %2 ms").arg(total).arg(elapsedMs));
}

void MainWindow::showToast(const QString &text, int level)
{
    ui::Toast::showMessage(this, text, static_cast<ui::Toast::Level>(level));
}

void MainWindow::setBusy(int done, int total)
{
    if (total <= 0) {
        m_progress->hide();
        return;
    }
    m_progress->setRange(0, total);
    m_progress->setValue(done);
    m_progress->show();
}

// ---------------------------------------------------------------------------
// 动效
// ---------------------------------------------------------------------------

void MainWindow::animatePage(QWidget *page)
{
    if (!page)
        return;

    auto *effect = new QGraphicsOpacityEffect(page);
    effect->setOpacity(0.0);
    page->setGraphicsEffect(effect);

    const QPoint target = page->pos();

    auto *group = new QParallelAnimationGroup(page);

    auto *fade = new QPropertyAnimation(effect, "opacity");
    fade->setDuration(200);
    fade->setStartValue(0.0);
    fade->setEndValue(1.0);
    fade->setEasingCurve(QEasingCurve::OutCubic);
    group->addAnimation(fade);

    auto *slide = new QPropertyAnimation(page, "pos");
    slide->setDuration(220);
    slide->setStartValue(target + QPoint(0, 12));
    slide->setEndValue(target);
    slide->setEasingCurve(QEasingCurve::OutCubic);
    group->addAnimation(slide);

    connect(group, &QAbstractAnimation::finished, page, [page, target]() {
        page->setGraphicsEffect(nullptr);
        page->move(target);
    });
    group->start(QAbstractAnimation::DeleteWhenStopped);
}

} // namespace das
