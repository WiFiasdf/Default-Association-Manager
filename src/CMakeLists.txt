set(DAS_CORE_SOURCES
    core/Types.h
    core/Category.h
    core/Category.cpp
    core/Logging.h
    core/Logging.cpp
    core/Paths.h
    core/Paths.cpp)

set(DAS_PLATFORM_SOURCES
    platform/win/RegKey.h
    platform/win/RegKey.cpp
    platform/win/WinUtils.h
    platform/win/WinUtils.cpp
    platform/win/AssocReader.h
    platform/win/AssocReader.cpp
    platform/win/ProgIdRegistrar.h
    platform/win/ProgIdRegistrar.cpp
    platform/win/UserChoiceHash.h
    platform/win/UserChoiceHash.cpp
    platform/win/UserChoiceWriter.h
    platform/win/UserChoiceWriter.cpp
    platform/win/ShellGuidedSetter.h
    platform/win/ShellGuidedSetter.cpp
    platform/win/IconProvider.h
    platform/win/IconProvider.cpp
    platform/win/WindowEffects.h
    platform/win/WindowEffects.cpp)

set(DAS_SERVICE_SOURCES
    services/IAssociationSetter.h
    services/HybridAssociationSetter.h
    services/HybridAssociationSetter.cpp
    services/AssociationService.h
    services/AssociationService.cpp
    services/ScanWorker.h
    services/ScanWorker.cpp
    services/BackupService.h
    services/BackupService.cpp
    services/ProfileService.h
    services/ProfileService.cpp)

set(DAS_UI_SOURCES
    ui/MainWindow.h
    ui/MainWindow.cpp
    ui/ThemeManager.h
    ui/ThemeManager.cpp
    ui/NavigationRail.h
    ui/NavigationRail.cpp
    ui/PageBase.h
    ui/Toast.h
    ui/Toast.cpp
    ui/UiKit.h
    ui/UiKit.cpp
    ui/pages/FileTypesPage.h
    ui/pages/FileTypesPage.cpp
    ui/pages/ProtocolsPage.h
    ui/pages/ProtocolsPage.cpp
    ui/pages/BackupPage.h
    ui/pages/BackupPage.cpp
    ui/pages/AboutPage.h
    ui/pages/AboutPage.cpp
    ui/models/AssociationTableModel.h
    ui/models/AssociationTableModel.cpp
    ui/models/AssociationFilterProxy.h
    ui/models/AssociationFilterProxy.cpp
    ui/delegates/AssociationItemDelegate.h
    ui/delegates/AssociationItemDelegate.cpp
    ui/dialogs/AppPickerDialog.h
    ui/dialogs/AppPickerDialog.cpp
    ui/dialogs/BatchAssignDialog.h
    ui/dialogs/BatchAssignDialog.cpp
    ui/dialogs/ResultReportDialog.h
    ui/dialogs/ResultReportDialog.cpp)

# 供单元测试复用的静态库（不含 main.cpp）
add_library(das_lib STATIC
    ${DAS_CORE_SOURCES}
    ${DAS_PLATFORM_SOURCES}
    ${DAS_SERVICE_SOURCES}
    ${DAS_UI_SOURCES})

target_include_directories(das_lib PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(das_lib
    PUBLIC
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        Qt6::Concurrent
        das_compile_options
    PRIVATE
        shell32
        shlwapi
        ole32
        oleaut32
        advapi32
        user32
        gdi32
        dwmapi
        propsys
        uuid)

if(Qt6Svg_FOUND)
    target_link_libraries(das_lib PUBLIC Qt6::Svg)
    target_compile_definitions(das_lib PUBLIC DAS_HAVE_QTSVG=1)
endif()

qt_add_resources(das_lib "das_assets"
    PREFIX "/"
    BASE "${CMAKE_CURRENT_SOURCE_DIR}/resources"
    FILES
        resources/qss/common.qss
        resources/qss/light.qss
        resources/qss/dark.qss
        resources/icons/nav_filetypes.svg
        resources/icons/nav_protocols.svg
        resources/icons/nav_backup.svg
        resources/icons/nav_about.svg
        resources/icons/menu.svg
        resources/icons/search.svg
        resources/icons/app.svg
        resources/icons/theme.svg
        resources/icons/refresh.svg
        resources/icons/check_light.svg
        resources/icons/check_dark.svg
        resources/icons/arrow_light.svg
        resources/icons/arrow_dark.svg)

# app.manifest 必须出现在源文件列表中：Qt 检测到自定义清单后会跳过自动生成，
# 链接器随后通过 /MANIFESTINPUT 将其嵌入可执行文件。
qt_add_executable(DefaultAppSetter WIN32
    main.cpp
    app.rc
    app.manifest)

set_target_properties(DefaultAppSetter PROPERTIES
    OUTPUT_NAME "默认软件设置器")

target_link_libraries(DefaultAppSetter PRIVATE das_lib)

# 注意：这里不能再加 /MANIFEST:NO，否则 app.manifest 无法被链接器嵌入。
# CMake 会为 .manifest 源文件自动追加 /MANIFEST:EMBED,ID=1 与 /MANIFESTINPUT。
