#pragma once

#include "core/Types.h"

#include <QHash>
#include <QString>
#include <QStringList>

namespace das::win {

/// 关联信息读取器。
///
/// 线程约定：每个使用本类的线程必须自行 CoInitializeEx；实例不可跨线程共享。
class AssocReader
{
public:
    AssocReader();
    ~AssocReader();

    AssocReader(const AssocReader &)            = delete;
    AssocReader &operator=(const AssocReader &) = delete;

    /// 本机全部文件扩展名（含前导点，小写，已去重并排序）。
    static QStringList enumerateExtensions();

    /// 常见 URL 协议列表（按重要度排序）。
    static QStringList wellKnownProtocols();

    /// 协议的中文说明。
    static QString protocolDescription(const QString &protocol);

    /// 当前生效的默认 ProgId。
    QString currentProgId(const QString &target, bool isProtocol);

    /// ProgId 对应的友好程序名（带内部缓存）。
    QString friendlyAppName(const QString &progId);

    /// ProgId 对应的可执行文件路径（带内部缓存）。
    QString executablePath(const QString &progId);

    /// ProgId 对应的文档类型说明（带内部缓存）。
    QString typeDescription(const QString &progId, const QString &extension);

    /// 组装一条完整的关联记录。
    AssociationEntry buildEntry(const QString &target, bool isProtocol);

    /// 汇总某个扩展名 / 协议的候选程序，已去重。
    AppCandidateList candidates(const QString &target, bool isProtocol);

    void clearCache();

private:
    struct ProgIdInfo {
        QString appName;
        QString exePath;
        QString docName;
        bool    resolved = false;
    };

    const ProgIdInfo &resolveProgId(const QString &progId);
    void              appendCandidate(AppCandidateList &list, const QString &progId);

    QHash<QString, ProgIdInfo> m_cache;
    void                      *m_aar = nullptr; ///< IApplicationAssociationRegistration*
};

} // namespace das::win
