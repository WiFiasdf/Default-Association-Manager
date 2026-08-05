#include "platform/win/UserChoiceWriter.h"

#include "core/Logging.h"
#include "platform/win/RegKey.h"
#include "platform/win/UserChoiceHash.h"
#include "platform/win/WinUtils.h"

#include <QHash>

namespace das::win {

namespace {

constexpr int kMaxAttempts = 3;

const QString kValueProgId = QStringLiteral("ProgId");
const QString kValueHash   = QStringLiteral("Hash");
const QString kSubKey      = QStringLiteral("UserChoice");

/// 抓取现有 UserChoice 键中除 ProgId / Hash 外的其它值。
/// 这样即便未来 Windows 新增字段，重建键后也能原样还原，而不是把它们弄丢。
QHash<QString, QString> snapshotExtraValues(const QString &parentPath)
{
    QHash<QString, QString> extras;
    RegKey key = RegKey::open(HKEY_CURRENT_USER, parentPath + QLatin1Char('\\') + kSubKey, KEY_READ);
    if (!key.isValid())
        return extras;

    const QStringList names = key.valueNames();
    for (const QString &name : names) {
        if (name.compare(kValueProgId, Qt::CaseInsensitive) == 0)
            continue;
        if (name.compare(kValueHash, Qt::CaseInsensitive) == 0)
            continue;
        bool          ok    = false;
        const QString value = key.readString(name, &ok);
        if (ok)
            extras.insert(name, value);
    }
    if (!extras.isEmpty())
        qCInfo(logReg) << "检测到 UserChoice 未知字段，将在重建后回写:" << extras.keys();
    return extras;
}

} // namespace

QString UserChoiceWriter::parentKeyPath(const QString &target, bool isProtocol)
{
    if (isProtocol) {
        return QStringLiteral("Software\\Microsoft\\Windows\\Shell\\Associations\\UrlAssociations\\%1")
            .arg(target);
    }
    return QStringLiteral("Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FileExts\\%1")
        .arg(target);
}

UserChoiceWriter::Outcome UserChoiceWriter::write(const QString &target, bool isProtocol,
                                                  const QString &progId)
{
    Outcome outcome;

    if (target.isEmpty() || progId.isEmpty()) {
        outcome.message = QStringLiteral("目标或程序标识为空。");
        return outcome;
    }

    const QString sid = currentUserSid();
    if (sid.isEmpty()) {
        outcome.message = QStringLiteral("无法获取当前用户 SID。");
        return outcome;
    }

    const QString parentPath = parentKeyPath(target, isProtocol);
    const QString childPath  = parentPath + QLatin1Char('\\') + kSubKey;

    const QHash<QString, QString> extras = snapshotExtraValues(parentPath);

    for (int attempt = 1; attempt <= kMaxAttempts; ++attempt) {
        // 1) 打开/创建父键
        RegKey parent = RegKey::create(HKEY_CURRENT_USER, parentPath,
                                       KEY_READ | KEY_WRITE | DELETE);
        if (!parent.isValid()) {
            outcome.winError = parent.status();
            outcome.message  = QStringLiteral("无法打开注册表父键：%1").arg(formatWinError(parent.status()));
            return outcome;
        }

        // 2) 删除旧的 UserChoice 子键。
        //    该子键上带有拒绝「设置值」的 ACE，但删除权限仍然保留，
        //    因此「删除 + 重建」是唯一可行的写入姿势。
        const LSTATUS delStatus = ::RegDeleteTreeW(parent.handle(), RegKey::wstr(kSubKey));
        if (delStatus != ERROR_SUCCESS && delStatus != ERROR_FILE_NOT_FOUND) {
            outcome.winError = delStatus;
            outcome.message  = QStringLiteral("无法清除旧的 UserChoice：%1").arg(formatWinError(delStatus));
            return outcome;
        }

        // 3) 重建空子键（继承的权限不含 Deny ACE）
        RegKey child = RegKey::create(HKEY_CURRENT_USER, childPath, KEY_READ | KEY_WRITE);
        if (!child.isValid()) {
            outcome.winError = child.status();
            outcome.message  = QStringLiteral("无法创建 UserChoice 键：%1").arg(formatWinError(child.status()));
            return outcome;
        }

        // 4) 取键的 LastWriteTime 并截断到分钟
        const quint64 stamp = truncateFileTimeToMinute(child.lastWriteTime());
        if (stamp == 0) {
            outcome.message = QStringLiteral("无法读取注册表键的时间戳。");
            return outcome;
        }

        // 5) 计算哈希
        const QString hash = userchoice::compute(target, sid, progId, stamp);
        if (hash.isEmpty()) {
            outcome.message = QStringLiteral("哈希计算失败。");
            return outcome;
        }

        // 6) 写值：先写未知字段，再写 ProgId 与 Hash
        for (auto it = extras.cbegin(); it != extras.cend(); ++it)
            child.writeString(it.key(), it.value());

        LSTATUS st = child.writeString(kValueProgId, progId);
        if (st == ERROR_SUCCESS)
            st = child.writeString(kValueHash, hash);

        if (st != ERROR_SUCCESS) {
            outcome.winError = st;
            outcome.message  = QStringLiteral("写入 UserChoice 失败：%1").arg(formatWinError(st));
            return outcome;
        }

        // 7) 写值会刷新 LastWriteTime。若跨过了分钟边界，说明刚才算的哈希已经作废，重来一次。
        const quint64 after = truncateFileTimeToMinute(child.lastWriteTime());
        if (after != stamp) {
            qCInfo(logReg) << "跨越分钟边界，重算 UserChoice 哈希，第" << attempt << "次";
            continue;
        }

        outcome.written = true;
        outcome.hash    = hash;
        outcome.message = QStringLiteral("已写入 UserChoice。");
        qCInfo(logReg).noquote() << "写入 UserChoice:" << target << "->" << progId
                                 << "sid=" << redactSid(sid)
                                 << "hash=" << redactHash(hash);

        notifyAssociationChanged();
        return outcome;
    }

    outcome.message = QStringLiteral("多次尝试后仍未能稳定写入 UserChoice。");
    return outcome;
}

UserChoiceWriter::Outcome UserChoiceWriter::remove(const QString &target, bool isProtocol)
{
    Outcome outcome;
    if (target.isEmpty()) {
        outcome.message = QStringLiteral("目标为空。");
        return outcome;
    }

    const QString parentPath = parentKeyPath(target, isProtocol);
    RegKey parent = RegKey::open(HKEY_CURRENT_USER, parentPath, KEY_READ | KEY_WRITE | DELETE);
    if (!parent.isValid()) {
        // 父键都不存在，说明本来就是系统默认
        outcome.written = true;
        outcome.message = QStringLiteral("当前已是系统默认。");
        return outcome;
    }

    const LSTATUS st = ::RegDeleteTreeW(parent.handle(), RegKey::wstr(kSubKey));
    if (st != ERROR_SUCCESS && st != ERROR_FILE_NOT_FOUND) {
        outcome.winError = st;
        outcome.message  = QStringLiteral("无法删除 UserChoice：%1").arg(formatWinError(st));
        return outcome;
    }

    outcome.written = true;
    outcome.message = QStringLiteral("已恢复为系统默认。");
    qCInfo(logReg) << "移除 UserChoice:" << target;
    notifyAssociationChanged();
    return outcome;
}

} // namespace das::win
