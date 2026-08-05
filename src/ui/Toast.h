#pragma once

#include <QFrame>
#include <QString>

class QLabel;

namespace das::ui {

/// 右下角轻量提示。自动堆叠、自动消失、点击可提前关闭。
///
/// 设计取向：常规反馈一律走 Toast，避免用模态弹窗打断操作；
/// 只有需要用户决策的场景才使用对话框。
class Toast : public QFrame
{
    Q_OBJECT

public:
    enum Level {
        Info = 0,
        Success,
        Warning,
        Error
    };

    /// anchor 可以是任意控件，提示会挂到它所在的顶层窗口上。
    static void showMessage(QWidget *anchor, const QString &text, Level level = Info,
                            int durationMs = 3200);

protected:
    void mousePressEvent(QMouseEvent *event) override;

private:
    Toast(QWidget *host, const QString &text, Level level);

    void        appear(int durationMs);
    void        dismiss();
    static void relayout(QWidget *host);

    QLabel *m_bar  = nullptr;
    QLabel *m_text = nullptr;
    bool    m_dismissing = false;
};

} // namespace das::ui
