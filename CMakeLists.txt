cmake_minimum_required(VERSION 3.21)

project(DefaultAppSetter
    VERSION 1.0.0
    DESCRIPTION "Windows 11 default application setter"
    LANGUAGES CXX)

if(NOT WIN32)
    message(FATAL_ERROR "DefaultAppSetter 仅支持 Windows 平台。")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

# ---------------------------------------------------------------------------
# Qt6 定位：优先使用调用方提供的 CMAKE_PREFIX_PATH，否则回退到本机常见安装位置
# ---------------------------------------------------------------------------
if(NOT CMAKE_PREFIX_PATH AND NOT DEFINED ENV{CMAKE_PREFIX_PATH})
    set(_qt_hints
        "D:/Qt/6.11.1/msvc2022_64"
        "D:/Qt/6.9.3/msvc2022_64"
        "C:/Qt/6.11.1/msvc2022_64"
        "C:/Qt/6.9.3/msvc2022_64")
    foreach(_hint IN LISTS _qt_hints)
        if(EXISTS "${_hint}/lib/cmake/Qt6/Qt6Config.cmake")
            set(CMAKE_PREFIX_PATH "${_hint}" CACHE PATH "Qt6 安装前缀" FORCE)
            message(STATUS "自动检测到 Qt6: ${_hint}")
            break()
        endif()
    endforeach()
endif()

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Widgets Concurrent)
qt_standard_project_setup(REQUIRES 6.5)

# Qt Svg 用于按主题重新着色矢量图标；缺失时回退到图片插件解码，不影响构建
find_package(Qt6 QUIET COMPONENTS Svg)
if(Qt6Svg_FOUND)
    message(STATUS "已启用 Qt6::Svg 图标着色")
else()
    message(STATUS "未找到 Qt6::Svg，图标着色改用 QImageReader 回退实现")
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

# ---------------------------------------------------------------------------
# 全局编译选项
# ---------------------------------------------------------------------------
add_library(das_compile_options INTERFACE)
target_compile_definitions(das_compile_options INTERFACE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    QT_DISABLE_DEPRECATED_BEFORE=0x060500
    DAS_VERSION_STRING="${PROJECT_VERSION}")

if(MSVC)
    target_compile_options(das_compile_options INTERFACE
        /utf-8
        /permissive-
        /Zc:__cplusplus
        /EHsc
        /MP
        /W4
        /wd4100   # 未引用的形参
        /wd4996)  # 已弃用的 CRT 函数
endif()

add_subdirectory(src)

option(DAS_BUILD_TESTS "构建单元测试" ON)
if(DAS_BUILD_TESTS)
    find_package(Qt6 QUIET COMPONENTS Test)
    if(Qt6Test_FOUND)
        enable_testing()
        add_subdirectory(tests)
    else()
        message(STATUS "未找到 Qt6::Test，跳过单元测试目标。")
    endif()
endif()
