#pragma once

#include "core/Types.h"

class QWidget;

namespace das {

/// 关联设置器抽象接口。便于替换策略与单元测试打桩。
class IAssociationSetter
{
public:
    virtual ~IAssociationSetter() = default;

    /// 设置文件扩展名的默认程序。
    virtual SetResult setFileType(const QString &extension, const QString &progId,
                                  QWidget *parent) = 0;

    /// 设置 URL 协议的默认程序。
    virtual SetResult setProtocol(const QString &protocol, const QString &progId,
                                  QWidget *parent) = 0;

    /// 恢复为系统默认（删除 UserChoice）。
    virtual SetResult restoreSystemDefault(const QString &target, bool isProtocol) = 0;
};

} // namespace das
