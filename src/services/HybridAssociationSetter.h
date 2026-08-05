#pragma once

#include "services/IAssociationSetter.h"

#include <memory>

namespace das {

namespace win {
class AssocReader;
}

/// 混合策略设置器：静默优先 → 回读校验 → 失败降级引导 → 二次校验。
///
/// 设计要点：写入成功不等于生效，资源管理器会校验哈希并静默重置非法的 UserChoice，
/// 因此**回读比对是唯一可靠的成功判据**，也是自动降级的触发条件。
class HybridAssociationSetter : public IAssociationSetter
{
public:
    HybridAssociationSetter();
    ~HybridAssociationSetter() override;

    SetResult setFileType(const QString &extension, const QString &progId, QWidget *parent) override;
    SetResult setProtocol(const QString &protocol, const QString &progId, QWidget *parent) override;
    SetResult restoreSystemDefault(const QString &target, bool isProtocol) override;

    /// 批量场景下不弹窗，仅做静默尝试；需要引导的项交由调用方统一收集后处理。
    void setSilentOnly(bool silentOnly) { m_silentOnly = silentOnly; }
    bool silentOnly() const { return m_silentOnly; }

    /// 手动为某一项触发引导流程（供结果面板中的「去设置」按钮使用）。
    SetResult runGuided(const QString &target, bool isProtocol, const QString &progId,
                        QWidget *parent);

private:
    SetResult apply(const QString &target, bool isProtocol, const QString &progId, QWidget *parent);

    std::unique_ptr<win::AssocReader> m_reader;
    bool                              m_silentOnly = false;
};

} // namespace das
