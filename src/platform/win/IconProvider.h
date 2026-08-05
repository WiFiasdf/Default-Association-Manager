#pragma once

#include <QCache>
#include <QHash>
#include <QIcon>
#include <QMutex>
#include <QObject>
#include <QSet>
#include <QString>
#include <QThreadPool>

namespace das::win {

/// 异步图标提供者。
///
/// 用法：视图在 Qt::DecorationRole 中调用 iconFor()，首次调用返回占位图并投递后台任务；
/// 任务完成后发出 iconReady(key)，模型据此局部刷新对应行。
class IconProvider : public QObject
{
    Q_OBJECT

public:
    static IconProvider &instance();

    /// key 语义：
    ///   - 扩展名（以 "." 开头），按文件类型取系统图标
    ///   - 可执行文件绝对路径，取该 exe 的图标
    QIcon iconFor(const QString &key);

    /// 同步取图标，仅用于对话框等非高频路径。
    QIcon iconForBlocking(const QString &key);

    QIcon placeholder() const { return m_placeholder; }

    void clear();

signals:
    void iconReady(const QString &key);

private slots:
    void onIconLoaded(const QString &key, const QImage &image);

private:
    explicit IconProvider(QObject *parent = nullptr);

    void request(const QString &key);

    mutable QMutex        m_mutex;
    QCache<QString, QIcon> m_cache;
    QSet<QString>          m_pending;
    QThreadPool           *m_pool = nullptr;
    QIcon                  m_placeholder;
};

} // namespace das::win
