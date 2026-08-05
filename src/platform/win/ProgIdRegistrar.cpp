#include "platform/win/ProgIdRegistrar.h"

#include "core/Logging.h"
#include "platform/win/RegKey.h"
#include "platform/win/WinUtils.h"

#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>

namespace das::win {

namespace {

const QString kClassesRoot = QStringLiteral("Software\\Classes");

QString sanitize(const QString &text)
{
    static const QRegularExpression invalid(QStringLiteral("[^A-Za-z0-9_]"));
    QString out = text;
    out.replace(invalid, QStringLiteral("_"));
    return out;
}

} // namespace

QString ProgIdRegistrar::makeProgIdName(const QString &exePath, const QString &target)
{
    const QFileInfo info(exePath);
    QString bare = target;
    if (bare.startsWith(QLatin1Char('.')))
        bare.remove(0, 1);
    return QStringLiteral("DefaultAppSetter.%1.%2")
        .arg(sanitize(info.completeBaseName()), sanitize(bare));
}

bool ProgIdRegistrar::progIdUsable(const QString &progId)
{
    if (progId.isEmpty())
        return false;
    bool          ok  = false;
    const QString cmd = RegKey::readStringValue(
        HKEY_CLASSES_ROOT, progId + QStringLiteral("\\shell\\open\\command"), QString(), &ok);
    return ok && !cmd.trimmed().isEmpty();
}

ProgIdRegistrar::Result ProgIdRegistrar::ensureProgId(const QString &exePath, const QString &target,
                                                      bool isProtocol)
{
    Result result;

    const QFileInfo info(exePath);
    if (exePath.isEmpty() || !info.exists() || !info.isFile()) {
        result.message = QStringLiteral("可执行文件不存在：%1").arg(exePath);
        return result;
    }

    const QString nativePath = QDir::toNativeSeparators(info.absoluteFilePath());
    const QString progId     = makeProgIdName(nativePath, target);
    const QString base       = kClassesRoot + QLatin1Char('\\') + progId;

    // 1) ProgId 根键：默认值为显示名称
    {
        RegKey key = RegKey::create(HKEY_CURRENT_USER, base, KEY_WRITE);
        if (!key.isValid()) {
            result.winError = key.status();
            result.message  = QStringLiteral("创建 ProgId 失败：%1").arg(formatWinError(key.status()));
            return result;
        }
        key.writeString(QString(), info.completeBaseName());
        key.writeString(QStringLiteral("FriendlyTypeName"), info.completeBaseName());
        if (isProtocol)
            key.writeString(QStringLiteral("URL Protocol"), QString());
    }

    // 2) DefaultIcon
    {
        RegKey key = RegKey::create(HKEY_CURRENT_USER, base + QStringLiteral("\\DefaultIcon"), KEY_WRITE);
        if (key.isValid())
            key.writeString(QString(), QStringLiteral("\"%1\",0").arg(nativePath));
    }

    // 3) shell\open\command
    {
        RegKey key = RegKey::create(HKEY_CURRENT_USER,
                                    base + QStringLiteral("\\shell\\open\\command"), KEY_WRITE);
        if (!key.isValid()) {
            result.winError = key.status();
            result.message  = QStringLiteral("写入启动命令失败：%1").arg(formatWinError(key.status()));
            return result;
        }
        // 命令行形如： "C:\Apps\foo.exe" "%1"
        const QString command = QLatin1Char('"') + nativePath + QLatin1String("\" \"%1\"");
        key.writeString(QString(), command);
    }

    // 4) 登记到扩展名的候选列表（协议不需要）
    if (!isProtocol) {
        RegKey extKey = RegKey::create(
            HKEY_CURRENT_USER,
            kClassesRoot + QLatin1Char('\\') + target + QStringLiteral("\\OpenWithProgids"), KEY_WRITE);
        if (extKey.isValid())
            extKey.writeString(progId, QString());
    }

    result.ok     = true;
    result.progId = progId;
    result.message = QStringLiteral("已为 %1 创建程序标识。").arg(info.fileName());
    qCInfo(logReg) << "创建 ProgId" << progId << "->" << nativePath;
    return result;
}

} // namespace das::win
