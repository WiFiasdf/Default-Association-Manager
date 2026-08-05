#include "services/HybridAssociationSetter.h"

#include "core/Logging.h"
#include "platform/win/AssocReader.h"
#include "platform/win/ShellGuidedSetter.h"
#include "platform/win/UserChoiceWriter.h"
#include "platform/win/WinUtils.h"

#include <QThread>

namespace das {

using win::AssocReader;
using win::ShellGuidedSetter;
using win::UserChoiceWriter;

HybridAssociationSetter::HybridAssociationSetter()
    : m_reader(std::make_unique<AssocReader>())
{
}

HybridAssociationSetter::~HybridAssociationSetter() = default;

SetResult HybridAssociationSetter::setFileType(const QString &extension, const QString &progId,
                                               QWidget *parent)
{
    return apply(extension, false, progId, parent);
}

SetResult HybridAssociationSetter::setProtocol(const QString &protocol, const QString &progId,
                                               QWidget *parent)
{
    return apply(protocol, true, progId, parent);
}

SetResult HybridAssociationSetter::apply(const QString &target, bool isProtocol,
                                         const QString &progId, QWidget *parent)
{
    SetResult result;
    result.target     = target;
    result.progId     = progId;
    result.isProtocol = isProtocol;
    result.appName    = m_reader->friendlyAppName(progId);

    if (target.isEmpty() || progId.isEmpty()) {
        result.status  = SetStatus::Failed;
        result.message = QStringLiteral("参数无效。");
        return result;
    }

    // 0) 已经是目标值？直接跳过，避免无谓写注册表
    if (m_reader->currentProgId(target, isProtocol).compare(progId, Qt::CaseInsensitive) == 0) {
        result.status  = SetStatus::Skipped;
        result.method  = SetMethod::None;
        result.message = QStringLiteral("已经是当前默认程序，无需更改。");
        return result;
    }

    // 1) 静默写入
    const UserChoiceWriter::Outcome outcome = UserChoiceWriter::write(target, isProtocol, progId);
    result.winError = outcome.winError;

    if (outcome.written) {
        // 2) 回读校验 —— 唯一可靠的成功判据
        m_reader->clearCache();
        const QString effective = m_reader->currentProgId(target, isProtocol);
        if (effective.compare(progId, Qt::CaseInsensitive) == 0) {
            result.status  = SetStatus::Success;
            result.method  = SetMethod::Silent;
            result.message = QStringLiteral("已静默生效。");
            return result;
        }
        qCWarning(logAssoc) << "静默写入后校验不通过:" << target
                            << "期望" << progId << "实际" << effective;
        result.message = QStringLiteral("静默写入未通过系统校验。");
    } else {
        qCWarning(logAssoc) << "静默写入失败:" << target << outcome.message;
        result.message = outcome.message;
    }

    // 3) 降级到引导
    if (m_silentOnly) {
        result.status  = SetStatus::NeedsUserConfirm;
        result.method  = SetMethod::Guided;
        result.message = QStringLiteral("静默设置未生效，需要在系统界面中手动确认。");
        return result;
    }

    return runGuided(target, isProtocol, progId, parent);
}

SetResult HybridAssociationSetter::runGuided(const QString &target, bool isProtocol,
                                             const QString &progId, QWidget *parent)
{
    SetResult result;
    result.target     = target;
    result.progId     = progId;
    result.isProtocol = isProtocol;
    result.appName    = m_reader->friendlyAppName(progId);
    result.method     = SetMethod::Guided;

    bool launched = false;
    if (isProtocol) {
        const QString appName = ShellGuidedSetter::registeredAppNameForProgId(progId);
        launched = ShellGuidedSetter::openDefaultAppsSettings(appName);
    } else {
        launched = ShellGuidedSetter::openWithDialog(parent, target);
    }

    if (!launched) {
        result.status  = SetStatus::Failed;
        result.message = QStringLiteral("已取消，或系统界面无法打开。");
        return result;
    }

    // 二次回读校验
    m_reader->clearCache();
    const QString effective = m_reader->currentProgId(target, isProtocol);
    if (effective.compare(progId, Qt::CaseInsensitive) == 0) {
        result.status  = SetStatus::Success;
        result.message = QStringLiteral("已通过系统界面设置成功。");
        win::notifyAssociationChanged();
        return result;
    }

    result.status  = SetStatus::NeedsUserConfirm;
    result.message = isProtocol ? QStringLiteral("已打开系统设置，请在其中选择目标程序。")
                                : QStringLiteral("系统对话框已关闭，但默认程序未变更。");
    return result;
}

SetResult HybridAssociationSetter::restoreSystemDefault(const QString &target, bool isProtocol)
{
    SetResult result;
    result.target     = target;
    result.isProtocol = isProtocol;
    result.method     = SetMethod::Silent;

    const UserChoiceWriter::Outcome outcome = UserChoiceWriter::remove(target, isProtocol);
    result.winError = outcome.winError;
    result.message  = outcome.message;
    result.status   = outcome.written ? SetStatus::Success : SetStatus::Failed;

    if (outcome.written) {
        m_reader->clearCache();
        result.progId  = m_reader->currentProgId(target, isProtocol);
        result.appName = m_reader->friendlyAppName(result.progId);
    }
    return result;
}

} // namespace das
