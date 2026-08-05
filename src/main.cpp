#include "core/Logging.h"
#include "core/Paths.h"
#include "core/Types.h"
#include "platform/win/WinUtils.h"
#include "ui/MainWindow.h"
#include "ui/ThemeManager.h"

#include <QApplication>
#include <QFont>
#include <QFontDatabase>
#include <QStringList>

#include <windows.h>
#include <objbase.h>

namespace {

void applyApplicationFont()
{
    // Win11 字体回退链：Segoe UI Variable Text -> Microsoft YaHei UI -> 思源黑体
    const QStringList families = QFontDatabase::families();
    QString primary = QStringLiteral("Microsoft YaHei UI");
    for (const QString &candidate : {QStringLiteral("Segoe UI Variable Text"),
                                     QStringLiteral("Microsoft YaHei UI"),
                                     QStringLiteral("Source Han Sans SC"),
                                     QStringLiteral("思源黑体"),
                                     QStringLiteral("Microsoft YaHei")}) {
        if (families.contains(candidate, Qt::CaseInsensitive)) {
            primary = candidate;
            break;
        }
    }

    QFont font(primary);
    font.setPointSizeF(10.0);
    font.setHintingPreference(QFont::PreferNoHinting);
    font.setFamilies({primary,
                      QStringLiteral("Microsoft YaHei UI"),
                      QStringLiteral("Source Han Sans SC"),
                      QStringLiteral("思源黑体"),
                      QStringLiteral("Segoe UI")});
    QApplication::setFont(font);
}

void registerMetaTypes()
{
    qRegisterMetaType<das::AssociationEntry>("das::AssociationEntry");
    qRegisterMetaType<das::AssociationEntryList>("das::AssociationEntryList");
    qRegisterMetaType<das::AppCandidate>("das::AppCandidate");
    qRegisterMetaType<das::SetResult>("das::SetResult");
    qRegisterMetaType<das::SetResultList>("das::SetResultList");
}

} // namespace

int main(int argc, char *argv[])
{
    const HRESULT comInit = ::CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("DefaultAppSetter"));
    app.setApplicationDisplayName(QStringLiteral("默认软件设置器"));
    app.setOrganizationName(QStringLiteral("DefaultAppSetter"));
    app.setApplicationVersion(QStringLiteral(DAS_VERSION_STRING));
    app.setWindowIcon(QIcon(QStringLiteral(":/icons/app.svg")));
    app.setQuitOnLastWindowClosed(true);

    das::installLogging();
    registerMetaTypes();
    applyApplicationFont();

    qCInfo(logUi).noquote() << "启动 默认软件设置器" << DAS_VERSION_STRING
                            << "| Windows Build" << das::win::realBuildNumber()
                            << "| 数据目录" << das::appDataDir();

    das::ThemeManager::instance().initialize();

    das::MainWindow window;
    window.show();

    const int code = app.exec();

    qCInfo(logUi) << "退出，返回码" << code;
    das::shutdownLogging();

    if (SUCCEEDED(comInit))
        ::CoUninitialize();
    return code;
}
