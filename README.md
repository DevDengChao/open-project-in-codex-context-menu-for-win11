# Open Project in Codex Context Menu for Windows 11

这个项目给 Windows 11 文件资源管理器补充 `Open project in Codex` 文件夹右键入口。

它同时覆盖两类入口：

- 对文件夹对象右键：`Directory\shell\OpenProjectInCodex`
- 在文件夹空白处右键：`Directory\Background\shell\OpenProjectInCodex`

Windows 11 新版右键菜单不只读取传统 `HKCU\Software\Classes\...\shell` 菜单项。要在新版菜单中稳定显示，需要一个带应用身份的 sparse package，并通过 `IExplorerCommand` COM DLL 暴露菜单命令。本项目就是把这部分整理成可以用 Visual Studio 打开的 C++ 工程。

## 目录结构

```text
.
├─ OpenProjectInCodexContextMenu.sln
├─ appx/
│  └─ AppxManifest.xml
├─ assets/
│  └─ codex-context-menu.png
├─ scripts/
│  ├─ build.ps1
│  ├─ register.ps1
│  ├─ smoke-test.ps1
│  └─ unregister.ps1
├─ src/
│  ├─ Host/
│  │  ├─ CodexContextMenuHost.cpp
│  │  └─ CodexContextMenuHost.vcxproj
│  └─ ShellExtension/
│     ├─ CodexContextMenu.cpp
│     ├─ CodexContextMenu.def
│     └─ CodexContextMenu.vcxproj
└─ tests/
   └─ ComSmoke/
      ├─ ComSmoke.vcxproj
      └─ TestCreate.cpp
```

## 构建要求

- Windows 11
- Visual Studio，已安装 `Desktop development with C++`
- Windows SDK
- Codex Desktop

本机已验证 Visual Studio 18 Community + `v145` 平台工具集可以构建。

## 用 Visual Studio 打开

打开这个 solution：

```powershell
.\OpenProjectInCodexContextMenu.sln
```

默认建议选择 `Release | x64`。

## 命令行构建

```powershell
.\scripts\build.ps1
```

构建完成后会生成：

- `build\Release\x64\CodexContextMenu.dll`
- `build\Release\x64\CodexContextMenuHost.exe`
- `build\Release\x64\ComSmoke.exe`
- `dist\sparse-package\` 下的 sparse package 注册目录

## 注册到当前用户

注册脚本会修改当前用户的 HKCU 注册表，并注册 sparse package。它不会修改 HKLM，也不会改 Codex Desktop 的安装目录。

```powershell
.\scripts\register.ps1 -RestartExplorer
```

注册后应能在以下位置看到 `Open project in Codex`：

- 文件夹对象右键菜单
- 文件夹空白处右键菜单
- Windows 11 新版右键菜单

如果菜单没有立即刷新，先关闭并重新打开资源管理器窗口；仍未刷新时再重启 Explorer。

## 卸载当前用户注册

```powershell
.\scripts\unregister.ps1 -RestartExplorer
```

这会移除：

- `OpenAI.CodexContextMenu` sparse package
- `HKCU\Software\Classes\Directory\shell\OpenProjectInCodex`
- `HKCU\Software\Classes\Directory\Background\shell\OpenProjectInCodex`
- `HKCU\Software\Classes\CLSID\{7F07B25C-22DE-46D3-9747-6B2D6B07F54D}`

## 验证

构建和暂存文件检查：

```powershell
.\scripts\smoke-test.ps1
```

注册后可以检查：

```powershell
reg query HKCU\Software\Classes\Directory\shell\OpenProjectInCodex /s
reg query HKCU\Software\Classes\Directory\Background\shell\OpenProjectInCodex /s
reg query HKCU\Software\Classes\CLSID\{7F07B25C-22DE-46D3-9747-6B2D6B07F54D} /s
Get-AppxPackage -Name OpenAI.CodexContextMenu
```

`tests\ComSmoke\ComSmoke.exe` 会通过当前 HKCU CLSID 注册创建 COM 对象，并读取菜单标题。它验证的是本机当前注册状态，不等同于自动安装。

## 排障 / Troubleshooting

常见问题先看这里，详细说明见 [TROUBLESHOOTING.md](./TROUBLESHOOTING.md)。

- 构建时报 `MSB8020`，通常表示当前机器缺少 `v145` 平台工具集，或未安装 `Desktop development with C++`。
- `register.ps1` 中的 `Add-AppxPackage` 报 `0x80073CFF`，通常表示未启用 Windows 开发者模式，未签名 sparse package 无法注册。
- `ComSmoke.exe` 在注册前报 `CoCreateInstance failed: 0x80040154` 属于预期现象，因为此时 HKCU CLSID 尚未注册。
- 已注册状态下重新跑 `build.ps1` 或 `smoke-test.ps1`，如果报找不到 `microsoft.system.package.metadata` 路径，通常表示构建脚本误删了 sparse package 根目录，而 Windows 正在维护该目录下的系统元数据。

## 实现说明

- COM CLSID：`{7F07B25C-22DE-46D3-9747-6B2D6B07F54D}`
- 菜单标题：`Open project in Codex`
- `IExplorerCommand::Invoke` 从 Explorer 传入的 `IShellItemArray` 读取目录路径。
- Win11 主菜单和经典菜单最终都会把目录转成 `codex://new?path=...` 并交给 Codex Desktop。
- 经典菜单通过 `CodexContextMenuHost.exe "%1"` / `"%V"` 转发目录参数，避免直接调用 `codex.exe app` 时弹出控制台窗口但未稳定打开项目的问题。
- 图标从 `Directory\Background\shell\OpenProjectInCodex` 的 `Icon` 值读取。
- sparse package 的 manifest 在 `appx\AppxManifest.xml`，构建脚本会把 DLL、宿主 EXE、PNG、ICO 和 manifest 复制到 `dist\sparse-package`，再由注册脚本使用 `Add-AppxPackage -Register -ExternalLocation` 注册。
- 经典菜单和 Win11 主菜单都不要直接依赖 `codex.exe` 取图标，当前实现统一使用 staging 目录下的 `codex-context-menu.ico`。

后续调整时优先保持“当前用户注册”和“sparse package staging 目录同级放置 manifest、DLL、EXE、PNG、ICO”的约定，这样能避免 WindowsApps 版本化路径和权限问题。
