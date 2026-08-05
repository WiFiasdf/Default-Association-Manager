#pragma once

#include <QString>
#include <QStringList>

#include <windows.h>

namespace das::win {

/// 注册表键的 RAII 封装。所有操作都返回明确的错误码，避免裸 LSTATUS 扩散到业务层。
class RegKey
{
public:
    RegKey() = default;
    explicit RegKey(HKEY handle) : m_key(handle) {}
    ~RegKey();

    RegKey(const RegKey &)            = delete;
    RegKey &operator=(const RegKey &) = delete;
    RegKey(RegKey &&other) noexcept;
    RegKey &operator=(RegKey &&other) noexcept;

    static RegKey open(HKEY root, const QString &subKey, REGSAM access = KEY_READ);
    static RegKey create(HKEY root, const QString &subKey, REGSAM access = KEY_ALL_ACCESS);

    bool    isValid() const { return m_key != nullptr; }
    HKEY    handle() const { return m_key; }
    LSTATUS status() const { return m_status; }
    void    close();
    HKEY    release();

    /// 枚举子键名（不含路径）。
    QStringList subKeyNames() const;
    /// 枚举值名（默认值以空字符串表示）。
    QStringList valueNames() const;

    bool    hasValue(const QString &name) const;
    QString readString(const QString &name = QString(), bool *ok = nullptr) const;
    LSTATUS writeString(const QString &name, const QString &value);
    LSTATUS writeExpandString(const QString &name, const QString &value);

    /// 键的最后写入时间，以 FILETIME 的 64 位整数表示（UTC）。失败返回 0。
    quint64 lastWriteTime() const;

    // --- 静态便捷方法 -------------------------------------------------------
    static bool        keyExists(HKEY root, const QString &subKey);
    static QStringList enumerateSubKeys(HKEY root, const QString &subKey);
    static QString     readStringValue(HKEY root, const QString &subKey,
                                       const QString &valueName = QString(), bool *ok = nullptr);
    static LSTATUS     deleteTree(HKEY root, const QString &subKey);

    /// 把 QString 转换成可直接传给 Win32 W 系列 API 的宽字符指针。
    static const wchar_t *wstr(const QString &s)
    {
        return reinterpret_cast<const wchar_t *>(s.utf16());
    }

private:
    HKEY    m_key    = nullptr;
    LSTATUS m_status = ERROR_SUCCESS;
};

} // namespace das::win
