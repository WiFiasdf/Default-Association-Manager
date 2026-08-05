#pragma once

#include <QString>
#include <QWidget>

namespace das::ui {

/// 所有内容页的公共基类，统一向主窗口上报状态与提示。
class PageBase : public QWidget
{
    Q_OBJECT

public:
    explicit PageBase(QWidget *parent = nullptr)
        : QWidget(parent)
    {
    }

    /// 页面被切换到前台时调用（可用于延迟加载）。
    virtual void onActivated() {}

    /// 窗口宽度不足时进入紧凑模式（隐藏次要信息）。
    virtual void setCompactMode(bool compact) { Q_UNUSED(compact) }

signals:
    /// 更新主窗口底部状态栏左侧文案。
    void statusMessage(const QString &text);

    /// 请求弹出 Toast。level 取值见 das::ui::Toast::Level。
    void toastRequested(const QString &text, int level);

    /// 长任务进度。total <= 0 表示结束。
    void busyChanged(int done, int total);
};

} // namespace das::ui
