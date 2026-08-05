#include "services/AssociationService.h"

#include "core/Logging.h"
#include "platform/win/AssocReader.h"
#include "platform/win/ProgIdRegistrar.h"
#include "platform/win/WinUtils.h"
#include "services/HybridAssociationSetter.h"
#include "services/ScanWorker.h"

#include <QCoreApplication>
#include <QThread>

namespace das {

AssociationService::AssociationService(QObject *parent)
    : QObject(parent),
      m_reader(std::make_unique<win::AssocReader>()),
      m_setter(std::make_unique<HybridAssociationSetter>()),
      m_backup(new BackupService(this)),
      m_profile(new ProfileService(this))
{
}

AssociationService::~AssociationService()
{
    if (m_scanThread) {
        if (m_worker)
            m_worker->cancel();
        m_scanThread->quit();
        m_scanThread->wait(3000);
    }
}

QString AssociationService::indexKey(const QString &target, bool isProtocol) const
{
    return (isProtocol ? QStringLiteral("p:") : QStringLiteral("f:")) + target.toLower();
}

// ---------------------------------------------------------------------------
// 扫描
// ---------------------------------------------------------------------------

void AssociationService::startScan()
{
    if (m_scanning)
        return;

    m_entries.clear();
    m_index.clear();
    m_reader->clearCache();
    m_scanning = true;

    m_scanThread = new QThread(this);
    m_worker     = new ScanWorker;
    m_worker->moveToThread(m_scanThread);

    connect(m_scanThread, &QThread::started, m_worker, &ScanWorker::run);
    connect(m_worker, &ScanWorker::scanStarted, this, &AssociationService::scanStarted);
    connect(m_worker, &ScanWorker::batchReady, this, &AssociationService::onBatchReady);
    connect(m_worker, &ScanWorker::scanFinished, this, &AssociationService::onScanFinished);
    connect(m_scanThread, &QThread::finished, m_worker, &QObject::deleteLater);

    m_scanThread->start();
}

void AssociationService::cancelScan()
{
    if (m_worker)
        m_worker->cancel();
}

void AssociationService::onBatchReady(const AssociationEntryList &batch)
{
    const int base = m_entries.size();
    m_entries.append(batch);
    for (int i = 0; i < batch.size(); ++i) {
        const AssociationEntry &entry = batch.at(i);
        m_index.insert(indexKey(entry.target, entry.isProtocol), base + i);
    }
    emit scanBatch(batch);
}

void AssociationService::onScanFinished(int total, qint64 elapsedMs, bool cancelled)
{
    m_scanning = false;

    if (m_scanThread) {
        m_scanThread->quit();
        m_scanThread->wait(3000);
        m_scanThread->deleteLater();
        m_scanThread = nullptr;
        m_worker     = nullptr;
    }
    emit scanFinished(total, elapsedMs, cancelled);
}

// ---------------------------------------------------------------------------
// 读取
// ---------------------------------------------------------------------------

AssociationEntry AssociationService::entryFor(const QString &target, bool isProtocol) const
{
    const int idx = m_index.value(indexKey(target, isProtocol), -1);
    if (idx >= 0 && idx < m_entries.size())
        return m_entries.at(idx);
    return {};
}

AssociationEntry AssociationService::reload(const QString &target, bool isProtocol)
{
    // 这里不清空读取器缓存：缓存键是 ProgId，而「ProgId → 程序名/路径」不会因为
    // 关联变化而失效；当前默认值本身是实时查询的。批量写入时清缓存会造成
    // O(n²) 级别的重复解析。
    AssociationEntry entry = m_reader->buildEntry(target, isProtocol);

    const int idx = m_index.value(indexKey(target, isProtocol), -1);
    if (idx >= 0 && idx < m_entries.size())
        m_entries[idx] = entry;

    return entry;
}

AppCandidateList AssociationService::candidatesFor(const QString &target, bool isProtocol)
{
    return m_reader->candidates(target, isProtocol);
}

QString AssociationService::ensureProgIdForExe(const QString &exePath, const QString &target,
                                               bool isProtocol, QString *error)
{
    const win::ProgIdRegistrar::Result result =
        win::ProgIdRegistrar::ensureProgId(exePath, target, isProtocol);
    if (!result.ok) {
        if (error)
            *error = result.message;
        return {};
    }
    m_reader->clearCache();
    return result.progId;
}

// ---------------------------------------------------------------------------
// 写入
// ---------------------------------------------------------------------------

void AssociationService::snapshotTargets(const TargetRefList &targets, const QString &note)
{
    if (targets.isEmpty())
        return;

    ProfileEntryList snapshot;
    snapshot.reserve(targets.size());
    for (const TargetRef &ref : targets) {
        ProfileEntry entry;
        entry.target     = ref.first;
        entry.isProtocol = ref.second;
        entry.progId     = m_reader->currentProgId(ref.first, ref.second);
        snapshot.append(entry);
    }
    m_backup->createSnapshot(snapshot, QStringLiteral("自动"), note);
}

void AssociationService::refreshEntryAfterWrite(const QString &target, bool isProtocol)
{
    const AssociationEntry entry = reload(target, isProtocol);
    emit entryChanged(entry);
}

SetResult AssociationService::applyOne(const QString &target, bool isProtocol,
                                       const QString &progId, QWidget *parent)
{
    snapshotTargets({{target, isProtocol}}, QStringLiteral("设置 %1 前").arg(target));

    m_setter->setSilentOnly(false);
    const SetResult result = isProtocol ? m_setter->setProtocol(target, progId, parent)
                                        : m_setter->setFileType(target, progId, parent);

    refreshEntryAfterWrite(target, isProtocol);
    return result;
}

SetResultList AssociationService::applyBatch(const TargetRefList &targets, const QString &progId,
                                             QWidget *parent)
{
    SetResultList results;
    if (targets.isEmpty())
        return results;

    snapshotTargets(targets, QStringLiteral("批量指派前（%1 项）").arg(targets.size()));

    // 批量场景禁止逐项弹模态框，需要引导的项统一收集，结束后在结果面板处理
    m_setter->setSilentOnly(true);
    results.reserve(targets.size());

    const int total = targets.size();
    for (int i = 0; i < total; ++i) {
        const TargetRef &ref = targets.at(i);
        SetResult result = ref.second ? m_setter->setProtocol(ref.first, progId, parent)
                                      : m_setter->setFileType(ref.first, progId, parent);
        results.append(result);
        refreshEntryAfterWrite(ref.first, ref.second);

        emit batchProgress(i + 1, total, result);
        if ((i % 8) == 0)
            QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents, 8);
    }
    m_setter->setSilentOnly(false);

    win::notifyAssociationChanged();
    return results;
}

SetResult AssociationService::restoreDefault(const QString &target, bool isProtocol)
{
    snapshotTargets({{target, isProtocol}}, QStringLiteral("恢复 %1 默认前").arg(target));
    const SetResult result = m_setter->restoreSystemDefault(target, isProtocol);
    refreshEntryAfterWrite(target, isProtocol);
    return result;
}

SetResultList AssociationService::restoreDefaults(const TargetRefList &targets)
{
    SetResultList results;
    if (targets.isEmpty())
        return results;

    snapshotTargets(targets, QStringLiteral("批量恢复默认前（%1 项）").arg(targets.size()));

    const int total = targets.size();
    results.reserve(total);
    for (int i = 0; i < total; ++i) {
        const TargetRef &ref = targets.at(i);
        SetResult result = m_setter->restoreSystemDefault(ref.first, ref.second);
        results.append(result);
        refreshEntryAfterWrite(ref.first, ref.second);

        emit batchProgress(i + 1, total, result);
        if ((i % 8) == 0)
            QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents, 8);
    }
    return results;
}

SetResultList AssociationService::applyProfile(const ProfileDiffList &diffs)
{
    TargetRefList targets;
    QHash<QString, QString> progIdByKey;

    for (const ProfileDiff &diff : diffs) {
        if (!diff.selected || diff.targetProgId.isEmpty())
            continue;
        targets.append({diff.target, diff.isProtocol});
        progIdByKey.insert(indexKey(diff.target, diff.isProtocol), diff.targetProgId);
    }

    SetResultList results;
    if (targets.isEmpty())
        return results;

    snapshotTargets(targets, QStringLiteral("套用方案前（%1 项）").arg(targets.size()));

    m_setter->setSilentOnly(true);
    const int total = targets.size();
    results.reserve(total);

    for (int i = 0; i < total; ++i) {
        const TargetRef &ref    = targets.at(i);
        const QString    progId = progIdByKey.value(indexKey(ref.first, ref.second));
        SetResult        result = ref.second ? m_setter->setProtocol(ref.first, progId, nullptr)
                                             : m_setter->setFileType(ref.first, progId, nullptr);
        results.append(result);
        refreshEntryAfterWrite(ref.first, ref.second);

        emit batchProgress(i + 1, total, result);
        if ((i % 8) == 0)
            QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents, 8);
    }
    m_setter->setSilentOnly(false);

    win::notifyAssociationChanged();
    return results;
}

SetResultList AssociationService::rollbackTo(const ProfileEntryList &entries)
{
    SetResultList results;
    if (entries.isEmpty())
        return results;

    TargetRefList targets;
    targets.reserve(entries.size());
    for (const ProfileEntry &entry : entries)
        targets.append({entry.target, entry.isProtocol});

    snapshotTargets(targets, QStringLiteral("回滚前（%1 项）").arg(targets.size()));

    m_setter->setSilentOnly(true);
    const int total = entries.size();
    results.reserve(total);

    for (int i = 0; i < total; ++i) {
        const ProfileEntry &entry = entries.at(i);
        SetResult result;
        if (entry.progId.isEmpty()) {
            // 快照记录当时没有默认程序 → 恢复系统默认
            result = m_setter->restoreSystemDefault(entry.target, entry.isProtocol);
        } else {
            result = entry.isProtocol
                         ? m_setter->setProtocol(entry.target, entry.progId, nullptr)
                         : m_setter->setFileType(entry.target, entry.progId, nullptr);
        }
        results.append(result);
        refreshEntryAfterWrite(entry.target, entry.isProtocol);

        emit batchProgress(i + 1, total, result);
        if ((i % 8) == 0)
            QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents, 8);
    }
    m_setter->setSilentOnly(false);

    win::notifyAssociationChanged();
    return results;
}

SetResult AssociationService::runGuided(const SetResult &item, QWidget *parent)
{
    const SetResult result =
        m_setter->runGuided(item.target, item.isProtocol, item.progId, parent);
    refreshEntryAfterWrite(item.target, item.isProtocol);
    return result;
}

// ---------------------------------------------------------------------------

ProfileEntryList AssociationService::currentProfile() const
{
    ProfileEntryList list;
    list.reserve(m_entries.size());
    for (const AssociationEntry &entry : m_entries) {
        if (entry.progId.isEmpty())
            continue;
        ProfileEntry item;
        item.target     = entry.target;
        item.progId     = entry.progId;
        item.isProtocol = entry.isProtocol;
        list.append(item);
    }
    return list;
}

} // namespace das
