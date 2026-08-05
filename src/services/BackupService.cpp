#include "services/BackupService.h"

#include "core/Logging.h"
#include "core/Paths.h"
#include "platform/win/WinUtils.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

namespace das {

QString BackupInfo::displayName() const
{
    return createdAt.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
}

BackupService::BackupService(QObject *parent) : QObject(parent) {}

bool BackupService::createSnapshot(const ProfileEntryList &entries, const QString &source,
                                   const QString &note, QString *outPath)
{
    if (entries.isEmpty()) {
        qCWarning(logAssoc) << "快照条目为空，跳过创建";
        return false;
    }

    const QDateTime now      = QDateTime::currentDateTime();
    const QString   fileName = now.toString(QStringLiteral("yyyyMMdd-HHmmss-zzz")) + QStringLiteral(".json");
    const QString   fullPath = backupDir() + QDir::separator() + fileName;

    QJsonArray array;
    for (const ProfileEntry &entry : entries) {
        QJsonObject item;
        item[QStringLiteral("target")]     = entry.target;
        item[QStringLiteral("progId")]     = entry.progId;
        item[QStringLiteral("isProtocol")] = entry.isProtocol;
        array.append(item);
    }

    QJsonObject root;
    root[QStringLiteral("schemaVersion")] = kSchemaVersion;
    root[QStringLiteral("kind")]          = QStringLiteral("snapshot");
    root[QStringLiteral("createdAt")]     = now.toString(Qt::ISODate);
    root[QStringLiteral("osBuild")]       = static_cast<int>(win::realBuildNumber());
    root[QStringLiteral("source")]        = source;
    root[QStringLiteral("note")]          = note;
    root[QStringLiteral("entries")]       = array;

    QSaveFile file(fullPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qCWarning(logAssoc) << "创建快照失败:" << fullPath << file.errorString();
        return false;
    }
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    if (!file.commit()) {
        qCWarning(logAssoc) << "写入快照失败:" << fullPath;
        return false;
    }

    qCInfo(logAssoc) << "已创建快照" << fileName << entries.size() << "条";
    if (outPath)
        *outPath = fullPath;

    pruneOldSnapshots();
    emit snapshotsChanged();
    return true;
}

BackupInfoList BackupService::listSnapshots() const
{
    BackupInfoList result;

    QDir dir(backupDir());
    const QFileInfoList files = dir.entryInfoList({QStringLiteral("*.json")}, QDir::Files, QDir::Name);
    result.reserve(files.size());

    for (const QFileInfo &info : files) {
        QFile file(info.absoluteFilePath());
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
            continue;

        QJsonParseError error{};
        const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
        file.close();
        if (error.error != QJsonParseError::NoError || !doc.isObject())
            continue;

        const QJsonObject root = doc.object();
        if (root.value(QStringLiteral("kind")).toString() != QLatin1String("snapshot"))
            continue;

        BackupInfo item;
        item.filePath   = info.absoluteFilePath();
        item.createdAt  = QDateTime::fromString(root.value(QStringLiteral("createdAt")).toString(),
                                                Qt::ISODate);
        if (!item.createdAt.isValid())
            item.createdAt = info.birthTime().isValid() ? info.birthTime() : info.lastModified();
        item.entryCount = root.value(QStringLiteral("entries")).toArray().size();
        item.source     = root.value(QStringLiteral("source")).toString(QStringLiteral("手动"));
        item.note       = root.value(QStringLiteral("note")).toString();
        result.append(item);
    }

    std::sort(result.begin(), result.end(), [](const BackupInfo &a, const BackupInfo &b) {
        return a.createdAt > b.createdAt;
    });
    return result;
}

ProfileEntryList BackupService::loadSnapshot(const QString &filePath, QString *error) const
{
    ProfileEntryList entries;

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (error)
            *error = QStringLiteral("无法打开快照文件：%1").arg(file.errorString());
        return entries;
    }

    QJsonParseError parseError{};
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    file.close();

    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        if (error)
            *error = QStringLiteral("快照文件格式损坏。");
        return entries;
    }

    const QJsonObject root    = doc.object();
    const int         version = root.value(QStringLiteral("schemaVersion")).toInt();
    if (version > kSchemaVersion) {
        if (error)
            *error = QStringLiteral("快照由更新版本的程序创建（v%1），当前版本不支持。").arg(version);
        return entries;
    }

    const QJsonArray array = root.value(QStringLiteral("entries")).toArray();
    entries.reserve(array.size());
    for (const QJsonValue &value : array) {
        const QJsonObject item = value.toObject();
        ProfileEntry      entry;
        entry.target     = item.value(QStringLiteral("target")).toString();
        entry.progId     = item.value(QStringLiteral("progId")).toString();
        entry.isProtocol = item.value(QStringLiteral("isProtocol")).toBool();
        if (!entry.target.isEmpty())
            entries.append(entry);
    }
    return entries;
}

bool BackupService::removeSnapshot(const QString &filePath)
{
    if (!QFile::remove(filePath))
        return false;
    emit snapshotsChanged();
    return true;
}

void BackupService::pruneOldSnapshots()
{
    BackupInfoList all = listSnapshots();
    if (all.size() <= kMaxBackups)
        return;

    for (int i = kMaxBackups; i < all.size(); ++i)
        QFile::remove(all.at(i).filePath);

    qCInfo(logAssoc) << "已清理" << (all.size() - kMaxBackups) << "个过期快照";
}

} // namespace das
