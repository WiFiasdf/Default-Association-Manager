#include "platform/win/RegKey.h"

#include <QVarLengthArray>

#include <cstring>

namespace das::win {

namespace {
constexpr DWORD kMaxKeyNameChars   = 256;  // MSDN: 注册表键名上限 255 字符
constexpr DWORD kMaxValueNameChars = 16384;
} // namespace

RegKey::~RegKey()
{
    close();
}

RegKey::RegKey(RegKey &&other) noexcept
    : m_key(other.m_key), m_status(other.m_status)
{
    other.m_key = nullptr;
}

RegKey &RegKey::operator=(RegKey &&other) noexcept
{
    if (this != &other) {
        close();
        m_key        = other.m_key;
        m_status     = other.m_status;
        other.m_key  = nullptr;
    }
    return *this;
}

void RegKey::close()
{
    if (m_key) {
        ::RegCloseKey(m_key);
        m_key = nullptr;
    }
}

HKEY RegKey::release()
{
    HKEY h = m_key;
    m_key  = nullptr;
    return h;
}

RegKey RegKey::open(HKEY root, const QString &subKey, REGSAM access)
{
    RegKey key;
    HKEY   handle = nullptr;
    key.m_status  = ::RegOpenKeyExW(root, subKey.isEmpty() ? nullptr : wstr(subKey),
                                    0, access, &handle);
    if (key.m_status == ERROR_SUCCESS)
        key.m_key = handle;
    return key;
}

RegKey RegKey::create(HKEY root, const QString &subKey, REGSAM access)
{
    RegKey key;
    HKEY   handle      = nullptr;
    DWORD  disposition = 0;
    key.m_status = ::RegCreateKeyExW(root, wstr(subKey), 0, nullptr,
                                     REG_OPTION_NON_VOLATILE, access, nullptr,
                                     &handle, &disposition);
    if (key.m_status == ERROR_SUCCESS)
        key.m_key = handle;
    return key;
}

QStringList RegKey::subKeyNames() const
{
    QStringList result;
    if (!m_key)
        return result;

    DWORD subKeyCount = 0;
    DWORD maxNameLen  = 0;
    if (::RegQueryInfoKeyW(m_key, nullptr, nullptr, nullptr, &subKeyCount, &maxNameLen,
                           nullptr, nullptr, nullptr, nullptr, nullptr, nullptr) != ERROR_SUCCESS) {
        return result;
    }
    if (subKeyCount == 0)
        return result;

    result.reserve(static_cast<int>(subKeyCount));

    const DWORD bufferChars = (maxNameLen ? maxNameLen : kMaxKeyNameChars) + 2;
    QVarLengthArray<wchar_t, 320> buffer(static_cast<qsizetype>(bufferChars));

    for (DWORD i = 0; i < subKeyCount; ++i) {
        DWORD  nameLen = bufferChars;
        LSTATUS st = ::RegEnumKeyExW(m_key, i, buffer.data(), &nameLen,
                                     nullptr, nullptr, nullptr, nullptr);
        if (st == ERROR_NO_MORE_ITEMS)
            break;
        if (st != ERROR_SUCCESS)
            continue;
        result.append(QString::fromWCharArray(buffer.data(), static_cast<int>(nameLen)));
    }
    return result;
}

QStringList RegKey::valueNames() const
{
    QStringList result;
    if (!m_key)
        return result;

    DWORD valueCount = 0;
    DWORD maxNameLen = 0;
    if (::RegQueryInfoKeyW(m_key, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
                           &valueCount, &maxNameLen, nullptr, nullptr, nullptr) != ERROR_SUCCESS) {
        return result;
    }
    if (valueCount == 0)
        return result;

    const DWORD bufferChars = (maxNameLen ? maxNameLen : kMaxValueNameChars) + 2;
    QVarLengthArray<wchar_t, 320> buffer(static_cast<qsizetype>(bufferChars));

    for (DWORD i = 0; i < valueCount; ++i) {
        DWORD   nameLen = bufferChars;
        LSTATUS st      = ::RegEnumValueW(m_key, i, buffer.data(), &nameLen,
                                          nullptr, nullptr, nullptr, nullptr);
        if (st == ERROR_NO_MORE_ITEMS)
            break;
        if (st != ERROR_SUCCESS)
            continue;
        result.append(QString::fromWCharArray(buffer.data(), static_cast<int>(nameLen)));
    }
    return result;
}

bool RegKey::hasValue(const QString &name) const
{
    if (!m_key)
        return false;
    DWORD   type = 0;
    DWORD   size = 0;
    LSTATUS st   = ::RegQueryValueExW(m_key, name.isEmpty() ? nullptr : wstr(name),
                                      nullptr, &type, nullptr, &size);
    return st == ERROR_SUCCESS;
}

QString RegKey::readString(const QString &name, bool *ok) const
{
    if (ok)
        *ok = false;
    if (!m_key)
        return {};

    DWORD   type = 0;
    DWORD   size = 0;
    LSTATUS st   = ::RegQueryValueExW(m_key, name.isEmpty() ? nullptr : wstr(name),
                                      nullptr, &type, nullptr, &size);
    if (st != ERROR_SUCCESS || size == 0)
        return {};
    if (type != REG_SZ && type != REG_EXPAND_SZ)
        return {};

    QVarLengthArray<BYTE, 512> buffer(static_cast<qsizetype>(size + sizeof(wchar_t)));
    std::memset(buffer.data(), 0, static_cast<size_t>(buffer.size()));

    DWORD readSize = size;
    st = ::RegQueryValueExW(m_key, name.isEmpty() ? nullptr : wstr(name), nullptr,
                            &type, buffer.data(), &readSize);
    if (st != ERROR_SUCCESS)
        return {};

    const auto *chars = reinterpret_cast<const wchar_t *>(buffer.data());
    QString     value = QString::fromWCharArray(chars);
    if (ok)
        *ok = true;
    return value;
}

LSTATUS RegKey::writeString(const QString &name, const QString &value)
{
    if (!m_key)
        return ERROR_INVALID_HANDLE;
    const auto  bytes = static_cast<DWORD>((value.size() + 1) * sizeof(wchar_t));
    m_status = ::RegSetValueExW(m_key, name.isEmpty() ? nullptr : wstr(name), 0, REG_SZ,
                                reinterpret_cast<const BYTE *>(wstr(value)), bytes);
    return m_status;
}

LSTATUS RegKey::writeExpandString(const QString &name, const QString &value)
{
    if (!m_key)
        return ERROR_INVALID_HANDLE;
    const auto bytes = static_cast<DWORD>((value.size() + 1) * sizeof(wchar_t));
    m_status = ::RegSetValueExW(m_key, name.isEmpty() ? nullptr : wstr(name), 0, REG_EXPAND_SZ,
                                reinterpret_cast<const BYTE *>(wstr(value)), bytes);
    return m_status;
}

quint64 RegKey::lastWriteTime() const
{
    if (!m_key)
        return 0;
    FILETIME ft{};
    if (::RegQueryInfoKeyW(m_key, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
                           nullptr, nullptr, nullptr, nullptr, &ft) != ERROR_SUCCESS) {
        return 0;
    }
    return (static_cast<quint64>(ft.dwHighDateTime) << 32) | ft.dwLowDateTime;
}

bool RegKey::keyExists(HKEY root, const QString &subKey)
{
    RegKey key = open(root, subKey, KEY_READ);
    return key.isValid();
}

QStringList RegKey::enumerateSubKeys(HKEY root, const QString &subKey)
{
    RegKey key = open(root, subKey, KEY_READ | KEY_ENUMERATE_SUB_KEYS);
    if (!key.isValid())
        return {};
    return key.subKeyNames();
}

QString RegKey::readStringValue(HKEY root, const QString &subKey, const QString &valueName, bool *ok)
{
    RegKey key = open(root, subKey, KEY_QUERY_VALUE);
    if (!key.isValid()) {
        if (ok)
            *ok = false;
        return {};
    }
    return key.readString(valueName, ok);
}

LSTATUS RegKey::deleteTree(HKEY root, const QString &subKey)
{
    return ::RegDeleteTreeW(root, wstr(subKey));
}

} // namespace das::win
