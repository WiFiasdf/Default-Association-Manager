#include "services/ScanWorker.h"

#include "core/Logging.h"
#include "platform/win/AssocReader.h"

#include <QElapsedTimer>

#include <objbase.h>

namespace das {

ScanWorker::ScanWorker(QObject *parent) : QObject(parent) {}

ScanWorker::~ScanWorker() = default;

void ScanWorker::cancel()
{
    m_cancelled.storeRelease(1);
}

void ScanWorker::run()
{
    m_cancelled.storeRelease(0);

    const HRESULT comInit = ::CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

    QElapsedTimer timer;
    timer.start();

    win::AssocReader reader;

    QStringList targets = win::AssocReader::wellKnownProtocols();
    const int   protocolCount = targets.size();
    targets += win::AssocReader::enumerateExtensions();

    emit scanStarted(targets.size());

    AssociationEntryList batch;
    batch.reserve(kBatchSize);

    int total = 0;
    for (int i = 0; i < targets.size(); ++i) {
        if (m_cancelled.loadAcquire()) {
            if (!batch.isEmpty())
                emit batchReady(batch);
            emit scanFinished(total, timer.elapsed(), true);
            if (SUCCEEDED(comInit))
                ::CoUninitialize();
            return;
        }

        const bool isProtocol = (i < protocolCount);
        batch.append(reader.buildEntry(targets.at(i), isProtocol));
        ++total;

        if (batch.size() >= kBatchSize) {
            emit batchReady(batch);
            batch.clear();
            batch.reserve(kBatchSize);
        }
    }

    if (!batch.isEmpty())
        emit batchReady(batch);

    const qint64 elapsed = timer.elapsed();
    qCInfo(logAssoc) << "扫描完成:" << total << "条, 耗时" << elapsed << "ms";
    emit scanFinished(total, elapsed, false);

    if (SUCCEEDED(comInit))
        ::CoUninitialize();
}

} // namespace das
