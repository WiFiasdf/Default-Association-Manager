#pragma once

#include "core/Types.h"

#include <QAtomicInt>
#include <QObject>

namespace das {

/// 后台全量扫描 worker。运行于独立 QThread，内部自行初始化 COM。
///
/// 性能约定：每 kBatchSize 条批量投递一次，避免逐条信号造成事件循环风暴。
class ScanWorker : public QObject
{
    Q_OBJECT

public:
    static constexpr int kBatchSize = 200;

    explicit ScanWorker(QObject *parent = nullptr);
    ~ScanWorker() override;

public slots:
    /// 在 worker 线程中执行全量扫描。
    void run();
    /// 请求取消（线程安全）。
    void cancel();

signals:
    void scanStarted(int total);
    void batchReady(const AssociationEntryList &batch);
    void scanFinished(int total, qint64 elapsedMs, bool cancelled);

private:
    QAtomicInt m_cancelled{0};
};

} // namespace das
