#include "services/ProfileService.h"

#include "core/Logging.h"
#include "platform/win/AssocReader.h"
#include "platform/win/WinUtils.h"

#include <QDateTime>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

namespace das {

ProfileService::ProfileService(QObject *parent) : QObject(parent) {}

bool ProfileService::exportProfile(const QString &filePath, const ProfileEntryList &entries,
                                   QString *error) const
{
    if (entries.isEmpty()) {
        if (error)
            *error = QStringLiteral("没有可导出的关联条目。");
        return false;
    }

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
    root[QStringLiteral("kind")]          = QStringLiteral("profile");
    root[QStringLiteral("createdAt")]     = QDateTime::currentDateTime().toString(Qt::ISODate);
    root[QStringLiteral("osBuild")]       = static_cast<int>(win::realBuildNumber());
    root[QStringLiteral("generator")]     = QStringLiteral("默认软件设置器 " DAS_VERSION_STRING);
    root[QStringLiteral("entries")]       = array;

    QSaveFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (error)
            *error = QStringLiteral("无法写入文件：%1").arg(file.errorString());
        return false;
    }
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    if (!file.commit()) {
        if (error)
            *error = QStringLiteral("保存文件失败。");
        return false;
    }

    qCInfo(logAssoc) << "已导出方案" << filePath << entries.size() << "条";
    return true;
}

ProfileEntryList ProfileService::importProfile(const QString &filePath, QString *error) const
{
    ProfileEntryList entries;

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (error)
            *error = QStringLiteral("无法打开文件：%1").arg(file.errorString());
        return entries;
    }

    QJsonParseError parseError{};
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    file.close();

    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        if (error)
            *error = QStringLiteral("文件不是有效的方案 JSON。");
        return entries;
    }

    const QJsonObject root    = doc.object();
    const int         version = root.value(QStringLiteral("schemaVersion")).toInt();
    if (version <= 0) {
        if (error)
            *error = QStringLiteral("缺少 schemaVersion 字段，无法识别的文件。");
        return entries;
    }
    if (version > kSchemaVersion) {
        if (error)
            *error = QStringLiteral("方案由更新版本的程序创建（v%1），请升级本工具。").arg(version);
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
        if (!entry.target.isEmpty() && !entry.progId.isEmpty())
            entries.append(entry);
    }

    if (entries.isEmpty() && error)
        *error = QStringLiteral("文件中没有有效的关联条目。");

    return entries;
}

ProfileDiffList ProfileService::diffAgainstCurrent(const ProfileEntryList &entries,
                                                   win::AssocReader &reader) const
{
    ProfileDiffList diffs;
    diffs.reserve(entries.size());

    for (const ProfileEntry &entry : entries) {
        ProfileDiff diff;
        diff.target        = entry.target;
        diff.isProtocol    = entry.isProtocol;
        diff.currentProgId = reader.currentProgId(entry.target, entry.isProtocol);
        diff.targetProgId  = entry.progId;

        if (!diff.changed())
            continue;

        diff.currentAppName = reader.friendlyAppName(diff.currentProgId);
        diff.targetAppName  = reader.friendlyAppName(diff.targetProgId);

        if (diff.currentAppName.isEmpty())
            diff.currentAppName = diff.currentProgId.isEmpty() ? QStringLiteral("未设置")
                                                               : diff.currentProgId;
        if (diff.targetAppName.isEmpty())
            diff.targetAppName = diff.targetProgId;

        diff.selected = true;
        diffs.append(diff);
    }

    return diffs;
}

} // namespace das
