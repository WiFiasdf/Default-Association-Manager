#include "platform/win/UserChoiceHash.h"

#include "platform/win/WinUtils.h"

#include <QByteArray>
#include <QCryptographicHash>

namespace das::win::userchoice {

namespace {

/// 以小端序读取 4 字节。
inline quint32 dwordAt(const QByteArray &data, int offset)
{
    if (offset < 0 || offset + 4 > data.size())
        return 0;
    const auto *p = reinterpret_cast<const quint8 *>(data.constData()) + offset;
    return static_cast<quint32>(p[0]) | (static_cast<quint32>(p[1]) << 8)
           | (static_cast<quint32>(p[2]) << 16) | (static_cast<quint32>(p[3]) << 24);
}

/// 以小端序写入 4 字节。
inline void putDword(QByteArray &data, int offset, quint32 value)
{
    auto *p = reinterpret_cast<quint8 *>(data.data()) + offset;
    p[0] = static_cast<quint8>(value & 0xFF);
    p[1] = static_cast<quint8>((value >> 8) & 0xFF);
    p[2] = static_cast<quint8>((value >> 16) & 0xFF);
    p[3] = static_cast<quint8>((value >> 24) & 0xFF);
}

/// 原实现中的 Get-ShiftRight(v, 16)：对 32 位值等价于无符号右移 16 位
/// （高位差异在后续乘法截断到低 32 位时被消除）。
inline quint32 shr16(quint32 value)
{
    return value >> 16;
}

/// UTF-16LE 编码，并保留结尾 NUL（算法要求）。
QByteArray toUtf16LeWithNul(const QString &text)
{
    const int bytes = (text.size() + 1) * 2;
    return QByteArray(reinterpret_cast<const char *>(text.utf16()), bytes);
}

} // namespace

QString experienceSalt()
{
    return QStringLiteral("User Choice set via Windows User Experience "
                          "{D18B6DD5-6124-4341-9318-804003BAFA0B}");
}

QString buildBaseInfo(const QString &target, const QString &userSid, const QString &progId,
                      quint64 fileTime)
{
    return (target + userSid + progId + formatFileTimeHex(fileTime) + experienceSalt()).toLower();
}

QString hashBaseInfo(const QString &baseInfo)
{
    if (baseInfo.isEmpty())
        return {};

    const QByteArray data = toUtf16LeWithNul(baseInfo);
    const QByteArray md5  = QCryptographicHash::hash(data, QCryptographicHash::Md5);
    if (md5.size() < 8)
        return {};

    const int lengthBase = data.size();                                 // == chars*2 + 2
    const int length     = ((lengthBase & 4) == 0 ? 1 : 0) + (lengthBase >> 2) - 1;
    if (length <= 1)
        return {};

    const int index    = (length - 2) >> 1;
    const int rounds   = index + 1;
    const int required = rounds * 8;
    if (required > data.size())
        return {};

    QByteArray outHash(16, '\0');

    // ---------------- 第一轮 ----------------
    {
        quint32 cache    = 0;
        quint32 outHash1 = 0;
        quint32 outHash2 = 0;
        const quint32 md51 = (dwordAt(md5, 0) | 1u) + 0x69FB0000u;
        const quint32 md52 = (dwordAt(md5, 4) | 1u) + 0x13DB0000u;

        int pdata = 0;
        for (int i = 0; i < rounds; ++i) {
            const quint32 r0 = dwordAt(data, pdata) + outHash1;
            const quint32 r1 = dwordAt(data, pdata + 4);
            pdata += 8;

            const quint32 r20 = r0 * md51 - 0x10FA9605u * shr16(r0);
            const quint32 r21 = 0x79F8A395u * r20 + 0x689B6B9Fu * shr16(r20);
            const quint32 r3  = 0xEA970001u * r21 - 0x3C101569u * shr16(r21);
            const quint32 r40 = r3 + r1;
            const quint32 r50 = cache + r3;
            const quint32 r60 = r40 * md52 - 0x3CE8EC25u * shr16(r40);
            const quint32 r61 = 0x59C3AF2Du * r60 - 0x2232E0F1u * shr16(r60);

            outHash1 = 0x1EC90001u * r61 + 0x35BD1EC9u * shr16(r61);
            outHash2 = r50 + outHash1;
            cache    = outHash2;
        }
        putDword(outHash, 0, outHash1);
        putDword(outHash, 4, outHash2);
    }

    // ---------------- 第二轮 ----------------
    {
        quint32 cache    = 0;
        quint32 outHash1 = 0;
        quint32 outHash2 = 0;
        const quint32 md51 = dwordAt(md5, 0) | 1u;
        const quint32 md52 = dwordAt(md5, 4) | 1u;

        int pdata = 0;
        for (int i = 0; i < rounds; ++i) {
            const quint32 r0 = dwordAt(data, pdata) + outHash1;
            pdata += 8;

            const quint32 r10 = r0 * md51;
            const quint32 r11 = 0xB1110000u * r10 - 0x30674EEFu * shr16(r10);
            const quint32 r20 = 0x5B9F0000u * r11 - 0x78F7A461u * shr16(r11);
            const quint32 r21 = 0x12CEB96Du * shr16(r20) - 0x46930000u * r20;
            const quint32 r3  = 0x1D830000u * r21 + 0x257E1D83u * shr16(r21);
            const quint32 r40 = md52 * (r3 + dwordAt(data, pdata - 4));
            const quint32 r41 = 0x16F50000u * r40 - 0x5D8BE90Bu * shr16(r40);
            const quint32 r50 = 0x96FF0000u * r41 - 0x2C7C6901u * shr16(r41);
            const quint32 r51 = 0x2B890000u * r50 + 0x7C932B89u * shr16(r50);

            outHash1 = 0x9F690000u * r51 - 0x405B6097u * shr16(r51);
            outHash2 = outHash1 + cache + r3;
            cache    = outHash2;
        }
        putDword(outHash, 8, outHash1);
        putDword(outHash, 12, outHash2);
    }

    // ---------------- 折叠为 8 字节 ----------------
    QByteArray result(8, '\0');
    putDword(result, 0, dwordAt(outHash, 8) ^ dwordAt(outHash, 0));
    putDword(result, 4, dwordAt(outHash, 12) ^ dwordAt(outHash, 4));

    return QString::fromLatin1(result.toBase64());
}

QString compute(const QString &target, const QString &userSid, const QString &progId,
                quint64 fileTime)
{
    return hashBaseInfo(buildBaseInfo(target, userSid, progId, fileTime));
}

} // namespace das::win::userchoice
