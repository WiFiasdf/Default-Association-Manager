#pragma once

#include <QWidget>

class QLabel;
class QProgressBar;
class QStackedWidget;
class QVBoxLayout;

namespace das {

class AssociationService;

namespace ui {
class AboutPage;
class BackupPage;
class FileTypesPage;
class IconButton;
class NavigationRail;
class PageBase;
class ProtocolsPage;
} // namespace ui

/// 主窗口：顶部命令区 + 左侧导航 + 内容栈 + 底部状态栏。
///
/// 保留系统原生标题栏（保证 Snap、多显示器 DPI、任务栏行为完全正常），
/// 再用 DWM 把标题栏配色刷成与应用一致，取得接近自绘的视觉连续性。
class MainWindow : public QWidget
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

protected:
    void resizeEvent(QResizeEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void closeEvent(QCloseEvent *event) override;

private slots:
    void onNavigationChanged(int index);
    void onThemeChanged(bool dark);
    void onScanStarted(int total);
    void onScanFinished(int total, qint64 elapsedMs, bool cancelled);

private:
    void buildUi();
    void buildTopBar(QVBoxLayout *rootLayout);
    void buildBody(QVBoxLayout *rootLayout);
    void buildStatusBar(QVBoxLayout *rootLayout);
    void registerPage(ui::PageBase *page, const QString &iconPath, const QString &title,
                      const QString &description);
    void connectService();
    void applyWindowEffects();
    void animatePage(QWidget *page);
    void updateResponsiveLayout();
    void showToast(const QString &text, int level);
    void setBusy(int done, int total);

    AssociationService *m_service = nullptr;

    ui::NavigationRail *m_rail  = nullptr;
    QStackedWidget     *m_stack = nullptr;

    ui::FileTypesPage *m_fileTypesPage = nullptr;
    ui::ProtocolsPage *m_protocolsPage = nullptr;
    ui::BackupPage    *m_backupPage    = nullptr;
    ui::AboutPage     *m_aboutPage     = nullptr;

    ui::IconButton *m_menuButton    = nullptr;
    ui::IconButton *m_refreshButton = nullptr;
    ui::IconButton *m_themeButton   = nullptr;

    QLabel       *m_statusText = nullptr;
    QLabel       *m_statusHint = nullptr;
    QProgressBar *m_progress   = nullptr;

    bool m_railManuallyCollapsed = false;
    bool m_firstShow             = true;
};

} // namespace das
