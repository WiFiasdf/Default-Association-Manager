#pragma once

#include "core/Types.h"

#include <QList>
#include <QString>

namespace das {

/// 依据扩展名（含前导点，大小写不敏感）判定所属类别，O(1) 查表。
FileCategory categoryForExtension(const QString &extension);

/// 界面筛选芯片的展示顺序。
const QList<FileCategory> &orderedCategories();

/// 为没有系统类型说明的扩展名生成一个兜底说明，如 ".png" -> "PNG 文件"。
QString fallbackTypeDescription(const QString &extension);

} // namespace das
