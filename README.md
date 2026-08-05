# 默认软件设置器 (Default App Setter)

一款面向 **Windows 11** 的默认打开方式管理工具，基于 **Qt 6 Widgets + MSVC** 构建。
用于批量、快捷地查看与修改文件扩展名与 URL 协议的默认程序，弥补系统设置中逐项手动点选的低效体验。

---

## 功能

| 模块 | 说明 |
| --- | --- |
| 文件类型关联 | 扫描本机全部扩展名，展示图标、类型说明与当前默认程序；支持关键词搜索与「图片/视频/音频/文档/代码/压缩包/可执行/其他」类别筛选 |
| 批量指派 | 多选扩展名一次性指派同一程序，逐项显示进度与结果 |
| 协议关联 | 管理 `http` / `https` / `mailto` / `ftp` 等协议，即默认浏览器与默认邮件客户端 |
| 备份与还原 | 每次设置前自动快照，可一键回滚到任意历史快照，或将选中项恢复为系统默认 |
| 方案导入导出 | 关联方案导出为 JSON，换机后导入并批量套用，导入前提供差异预览 |

## 设置策略：混合模式

Windows 10/11 的文件关联受 `UserChoice` 键下的 `Hash` 校验值保护，微软没有公开写入 API
（`IApplicationAssociationRegistration::SetAppAsDefault` 自 Win10 起已被禁用）。

本工具采用**静默优先 + 引导回退**的混合策略：

```
用户发起设置
   → 确保目标 ProgId 存在（必要时在 HKCU\Software\Classes 下自建）
   → 写前自动快照
   → 计算 UserChoice 哈希并写入注册表（静默）
   → 回读校验（QueryCurrentDefault）
        ├─ 一致  → SHChangeNotify 通知资源管理器，标记「已生效」
        └─ 不一致 → 打开系统「打开方式」对话框 / ms-settings 深链
                   → 二次回读校验 → 标记「需手动确认」或「失败」
```

**关键点**：写入成功 ≠ 生效。资源管理器会校验哈希并静默重置不合法的 `UserChoice`，
因此回读比对是唯一可靠的成功判据，也是自动降级的触发条件。

## 权限

所有关联写入均在 **HKCU** 作用域内完成，**无需管理员权限**，清单声明为 `asInvoker`。

---

## 构建

### 环境要求

- Windows 10 / 11 x64
- Qt 6.5+ 的 `msvc2022_64` 套件（本项目在 **Qt 6.11.1** 上验证）
- Visual Studio 2022 或更新版本的 MSVC 工具链
- CMake 3.21+、Ninja（Qt 安装器自带于 `<QtRoot>\Tools\`）

### 一键构建

```powershell
# 默认 Release 构建
.\build.ps1

# Debug 构建
.\build.ps1 -Config Debug

# 清理重建 + 跑单元测试 + 生成绿色包
.\build.ps1 -Clean -RunTests -Deploy

# 仅改了 .h/.cpp 时跳过 CMake 配置阶段，直接增量编译（更快）
.\build.ps1 -NoConfigure
```

脚本会自动：定位 Qt → 定位 CMake/Ninja → 导入 `vcvars64.bat` 环境 → CMake 配置 → 编译。
若自动定位失败，可显式指定：

```powershell
.\build.ps1 -QtDir 'D:\Qt\6.11.1\msvc2022_64' -VsDir 'D:\Program Files\Microsoft Visual Studio\18\Community'
```

### 绿色免安装打包

```powershell
.\deploy.ps1
```

产物位于 `dist\DefaultAppSetter\`，整个目录可直接拷贝到其他机器运行。

### 用 Qt Creator 打开

直接打开根目录 `CMakeLists.txt`，选择 `Desktop Qt 6.11.1 MSVC2022 64bit` 套件即可。
工程同时提供 `CMakePresets.json`（`ninja-msvc-release` / `ninja-msvc-debug`）。

---

## 工程结构

```
src/
├── core/          数据结构、类别映射、日志、路径
├── platform/win/  注册表 RAII、SID/时间戳、关联读取、UserChoice 哈希与写入、
│                  ProgId 注册、系统引导、异步图标、DWM 窗口效果
├── services/      混合设置引擎、业务门面、后台扫描、备份、方案导入导出
├── ui/            主窗口、导航栏、主题、4 个页面、模型/代理/委托、3 个对话框
└── resources/     QSS 主题与 SVG 图标
tests/             UserChoice 哈希与方案服务单元测试
```

## 数据位置

| 内容 | 路径 |
| --- | --- |
| 备份快照 | `%APPDATA%\DefaultAppSetter\DefaultAppSetter\backups\*.json` |
| 运行日志 | `%APPDATA%\DefaultAppSetter\DefaultAppSetter\logs\app.log`（2 MB 滚动，保留 3 份） |

> 关于页提供「打开日志目录」「打开备份目录」按钮，可直接定位。

日志中的用户 SID 仅保留末 4 位，哈希仅保留前 4 字符。

---

## 风险与免责声明

- 静默设置依赖对微软**未公开**的 `UserChoice` 哈希算法的社区逆向实现。
  微软可能在未来的 Windows 更新中修改该算法，届时本工具会自动降级为系统引导方式，功能不会中断。
- 个别安全软件可能对"直接写入 UserChoice"的行为发出提示，这是该类工具的共性。
- 本工具仅写入 `HKCU`，不修改任何系统级配置；每次设置前都会自动快照，可随时回滚。
- 本项目按「原样」提供，使用者需自行承担风险。

## 许可

MIT License
