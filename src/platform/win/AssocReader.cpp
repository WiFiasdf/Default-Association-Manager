#include "platform/win/AssocReader.h"

#include "core/Category.h"
#include "core/Logging.h"
#include "platform/win/RegKey.h"
#include "platform/win/WinUtils.h"

#include <QDir>
#include <QFileInfo>
#include <QSet>
#include <QVarLengthArray>

#include <shlobj.h>
#include <shlwapi.h>
#include <shobjidl.h>

namespace das::win {

namespace {

constexpr const wchar_t *kFileExtsPath =
    L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FileExts";

QString assocQuery(ASSOCSTR what, const QString &assoc, ASSOCF flags = ASSOCF_NONE)
{
    if (assoc.isEmpty())
        return {};

    DWORD size = 0;
    HRESULT hr = ::AssocQueryStringW(flags, what, RegKey::wstr(assoc), L"open", nullptr, &size);
    if (FAILED(hr) || size == 0)
        return {};

    QVarLengthArray<wchar_t, 512> buffer(static_cast<qsizetype>(size + 1));
    hr = ::AssocQueryStringW(flags, what, RegKey::wstr(assoc), L"open", buffer.data(), &size);
    if (FAILED(hr))
        return {};

    return QString::fromWCharArray(buffer.data()).trimmed();
}

/// 合并 HKLM\Software\Classes 与 HKCU\Software\Classes 下以 "." 开头的键。
/// 比直接枚举 HKEY_CLASSES_ROOT 合并视图快得多。
void collectClassExtensions(HKEY root, const QString &subKey, QSet<QString> &out)
{
    const QStringList names = RegKey::enumerateSubKeys(root, subKey);
    for (const QString &name : names) {
        if (name.size() < 2 || !name.startsWith(QLatin1Char('.')))
            continue;
        if (name.contains(QLatin1Char(' ')))
            continue;
        out.insert(name.toLower());
    }
}

QString progIdCommand(const QString &progId)
{
    if (progId.isEmpty())
        return {};
    const QString path = progId + QStringLiteral("\\shell\\open\\command");
    bool          ok   = false;
    QString       cmd  = RegKey::readStringValue(HKEY_CLASSES_ROOT, path, QString(), &ok);
    return ok ? cmd : QString();
}

} // namespace

// ---------------------------------------------------------------------------

AssocReader::AssocReader()
{
    IApplicationAssociationRegistration *aar = nullptr;
    const HRESULT hr = ::CoCreateInstance(CLSID_ApplicationAssociationRegistration, nullptr,
                                          CLSCTX_INPROC_SERVER,
                                          IID_PPV_ARGS(&aar));
    if (SUCCEEDED(hr))
        m_aar = aar;
    else
        qCWarning(logAssoc) << "创建 IApplicationAssociationRegistration 失败, hr=" << Qt::hex << hr;
}

AssocReader::~AssocReader()
{
    if (m_aar) {
        static_cast<IApplicationAssociationRegistration *>(m_aar)->Release();
        m_aar = nullptr;
    }
}

QStringList AssocReader::enumerateExtensions()
{
    QSet<QString> set;
    collectClassExtensions(HKEY_LOCAL_MACHINE, QStringLiteral("SOFTWARE\\Classes"), set);
    collectClassExtensions(HKEY_CURRENT_USER, QStringLiteral("Software\\Classes"), set);
    collectClassExtensions(HKEY_CURRENT_USER,
                           QString::fromWCharArray(kFileExtsPath), set);

    QStringList list(set.cbegin(), set.cend());
    list.sort(Qt::CaseInsensitive);
    return list;
}

QStringList AssocReader::wellKnownProtocols()
{
    return {QStringLiteral("http"),   QStringLiteral("https"), QStringLiteral("mailto"),
            QStringLiteral("ftp"),    QStringLiteral("tel"),   QStringLiteral("callto"),
            QStringLiteral("webcal"), QStringLiteral("news"),  QStringLiteral("nntp"),
            QStringLiteral("sms"),    QStringLiteral("irc"),   QStringLiteral("magnet")};
}

QString AssocReader::protocolDescription(const QString &protocol)
{
    static const QHash<QString, QString> map{
        {QStringLiteral("http"),   QStringLiteral("网页浏览（HTTP）")},
        {QStringLiteral("https"),  QStringLiteral("安全网页浏览（HTTPS）")},
        {QStringLiteral("mailto"), QStringLiteral("电子邮件")},
        {QStringLiteral("ftp"),    QStringLiteral("文件传输协议")},
        {QStringLiteral("tel"),    QStringLiteral("电话拨号")},
        {QStringLiteral("callto"), QStringLiteral("网络通话")},
        {QStringLiteral("webcal"), QStringLiteral("网络日历订阅")},
        {QStringLiteral("news"),   QStringLiteral("新闻组")},
        {QStringLiteral("nntp"),   QStringLiteral("新闻组传输")},
        {QStringLiteral("sms"),    QStringLiteral("短消息")},
        {QStringLiteral("irc"),    QStringLiteral("IRC 聊天")},
        {QStringLiteral("magnet"), QStringLiteral("磁力链接下载")},
    };
    return map.value(protocol.toLower(), QStringLiteral("URL 协议"));
}

QString AssocReader::currentProgId(const QString &target, bool isProtocol)
{
    if (target.isEmpty())
        return {};

    // 1) 首选官方读取接口
    if (m_aar) {
        auto   *aar = static_cast<IApplicationAssociationRegistration *>(m_aar);
        LPWSTR  out = nullptr;
        const HRESULT hr = aar->QueryCurrentDefault(RegKey::wstr(target),
                                                    isProtocol ? AT_URLPROTOCOL : AT_FILEEXTENSION,
                                                    AL_EFFECTIVE, &out);
        if (SUCCEEDED(hr) && out) {
            QString value = QString::fromWCharArray(out);
            ::CoTaskMemFree(out);
            if (!value.isEmpty())
                return value;
        }
    }

    // 2) 回退：直接读 UserChoice
    const QString userChoicePath =
        isProtocol
            ? QStringLiteral("Software\\Microsoft\\Windows\\Shell\\Associations\\UrlAssociations\\%1\\UserChoice")
                  .arg(target)
            : QStringLiteral("Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FileExts\\%1\\UserChoice")
                  .arg(target);

    bool    ok    = false;
    QString value = RegKey::readStringValue(HKEY_CURRENT_USER, userChoicePath,
                                            QStringLiteral("ProgId"), &ok);
    if (ok && !value.isEmpty())
        return value;

    // 3) 再回退：HKCR\<target> 的默认值
    value = RegKey::readStringValue(HKEY_CLASSES_ROOT, target, QString(), &ok);
    return ok ? value : QString();
}

const AssocReader::ProgIdInfo &AssocReader::resolveProgId(const QString &progId)
{
    auto it = m_cache.find(progId);
    if (it != m_cache.end())
        return it.value();

    ProgIdInfo info;
    info.resolved = true;

    if (!progId.isEmpty()) {
        info.appName = assocQuery(ASSOCSTR_FRIENDLYAPPNAME, progId);
        info.exePath = assocQuery(ASSOCSTR_EXECUTABLE, progId);
        info.docName = assocQuery(ASSOCSTR_FRIENDLYDOCNAME, progId);

        if (info.exePath.isEmpty())
            info.exePath = extractExecutableFromCommand(progIdCommand(progId));

        if (info.appName.isEmpty() && !info.exePath.isEmpty())
            info.appName = QFileInfo(info.exePath).completeBaseName();

        if (info.appName.isEmpty()) {
            bool ok = false;
            const QString display = RegKey::readStringValue(HKEY_CLASSES_ROOT, progId, QString(), &ok);
            if (ok && !display.isEmpty())
                info.appName = display;
        }

        if (!info.exePath.isEmpty())
            info.exePath = QDir::toNativeSeparators(info.exePath);
    }

    return *m_cache.insert(progId, info);
}

QString AssocReader::friendlyAppName(const QString &progId)
{
    return resolveProgId(progId).appName;
}

QString AssocReader::executablePath(const QString &progId)
{
    return resolveProgId(progId).exePath;
}

QString AssocReader::typeDescription(const QString &progId, const QString &extension)
{
    const QString doc = resolveProgId(progId).docName;
    if (!doc.isEmpty())
        return doc;
    return fallbackTypeDescription(extension);
}

AssociationEntry AssocReader::buildEntry(const QString &target, bool isProtocol)
{
    AssociationEntry entry;
    entry.target     = target;
    entry.isProtocol = isProtocol;
    entry.progId     = currentProgId(target, isProtocol);
    entry.appName    = friendlyAppName(entry.progId);
    entry.appPath    = executablePath(entry.progId);
    entry.category   = isProtocol ? FileCategory::Other : categoryForExtension(target);
    entry.typeDescription = isProtocol ? protocolDescription(target)
                                       : typeDescription(entry.progId, target);

    if (entry.appName.isEmpty())
        entry.appName = entry.progId.isEmpty() ? QStringLiteral("未设置") : entry.progId;

    return entry;
}

void AssocReader::appendCandidate(AppCandidateList &list, const QString &progId)
{
    if (progId.isEmpty())
        return;
    for (const AppCandidate &existing : list) {
        if (existing.progId.compare(progId, Qt::CaseInsensitive) == 0)
            return;
    }

    AppCandidate candidate;
    candidate.progId      = progId;
    candidate.displayName = friendlyAppName(progId);
    candidate.exePath     = executablePath(progId);
    if (candidate.displayName.isEmpty())
        candidate.displayName = progId;
    // 完全解析不出可执行文件的 ProgId 通常是无效残留，丢弃
    if (candidate.exePath.isEmpty())
        return;
    list.append(candidate);
}

AppCandidateList AssocReader::candidates(const QString &target, bool isProtocol)
{
    AppCandidateList result;
    if (target.isEmpty())
        return result;

    const QString current = currentProgId(target, isProtocol);
    appendCandidate(result, current);

    if (!isProtocol) {
        // HKCR\<ext>\OpenWithProgids
        {
            RegKey key = RegKey::open(HKEY_CLASSES_ROOT,
                                      target + QStringLiteral("\\OpenWithProgids"), KEY_READ);
            if (key.isValid()) {
                for (const QString &name : key.valueNames())
                    appendCandidate(result, name);
            }
        }
        // HKCU\...\FileExts\<ext>\OpenWithProgids
        {
            RegKey key = RegKey::open(
                HKEY_CURRENT_USER,
                QStringLiteral("Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FileExts\\%1\\OpenWithProgids")
                    .arg(target),
                KEY_READ);
            if (key.isValid()) {
                for (const QString &name : key.valueNames())
                    appendCandidate(result, name);
            }
        }
        // HKCR\<ext> 默认 ProgId
        {
            bool          ok      = false;
            const QString defProg = RegKey::readStringValue(HKEY_CLASSES_ROOT, target, QString(), &ok);
            if (ok)
                appendCandidate(result, defProg);
        }
    } else {
        // 浏览器 / 邮件客户端：从 StartMenuInternet 与 Clients\Mail 收集
        const QString clientsRoot = (target.compare(QLatin1String("mailto"), Qt::CaseInsensitive) == 0)
                                        ? QStringLiteral("SOFTWARE\\Clients\\Mail")
                                        : QStringLiteral("SOFTWARE\\Clients\\StartMenuInternet");
        for (HKEY root : {HKEY_LOCAL_MACHINE, HKEY_CURRENT_USER}) {
            const QStringList clients = RegKey::enumerateSubKeys(root, clientsRoot);
            for (const QString &client : clients) {
                bool          ok  = false;
                const QString cmd = RegKey::readStringValue(
                    root, QStringLiteral("%1\\%2\\shell\\open\\command").arg(clientsRoot, client),
                    QString(), &ok);
                if (!ok || cmd.isEmpty())
                    continue;
                // 通过 Capabilities 找到该客户端为此协议注册的 ProgId
                const QString progId = RegKey::readStringValue(
                    root,
                    QStringLiteral("%1\\%2\\Capabilities\\URLAssociations").arg(clientsRoot, client),
                    target, &ok);
                if (ok && !progId.isEmpty())
                    appendCandidate(result, progId);
            }
        }
    }

    // RegisteredApplications 中声明的能力
    for (HKEY root : {HKEY_LOCAL_MACHINE, HKEY_CURRENT_USER}) {
        RegKey regApps = RegKey::open(root, QStringLiteral("SOFTWARE\\RegisteredApplications"), KEY_READ);
        if (!regApps.isValid())
            continue;
        const QStringList appNames = regApps.valueNames();
        for (const QString &appName : appNames) {
            bool          ok            = false;
            const QString capabilityKey = regApps.readString(appName, &ok);
            if (!ok || capabilityKey.isEmpty())
                continue;
            const QString assocKey = capabilityKey
                                     + (isProtocol ? QStringLiteral("\\URLAssociations")
                                                   : QStringLiteral("\\FileAssociations"));
            const QString progId = RegKey::readStringValue(root, assocKey, target, &ok);
            if (ok && !progId.isEmpty())
                appendCandidate(result, progId);
        }
    }

    for (AppCandidate &candidate : result)
        candidate.isCurrent = (candidate.progId.compare(current, Qt::CaseInsensitive) == 0);

    return result;
}

void AssocReader::clearCache()
{
    m_cache.clear();
}

} // namespace das::win
