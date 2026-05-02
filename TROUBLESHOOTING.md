# Troubleshooting

这份文档总结了本项目在本机验证时实际遇到过的问题，按“现象 -> 原因 -> 处理”的方式组织，方便后续快速排查。

## 已验证环境

- Windows 11
- Visual Studio Community 2026 18.5.2
- 已安装 `Desktop development with C++`
- 已具备 `v145` 平台工具集
- 已安装 Windows SDK

以上环境下，`.\scripts\build.ps1` 可以成功构建。

## 构建阶段

### 现象：`build.ps1` 报 `MSB8020`

典型报错：

```text
error MSB8020: 无法找到 v145 的生成工具(平台工具集 ="v145")
```

原因：

- 当前机器缺少 `v145` 平台工具集。
- 或者未安装 `Desktop development with C++`，导致 C++ 工具链不完整。

处理：

1. 打开 Visual Studio Installer。
2. 确认当前实例已安装 `Desktop development with C++`。
3. 确认 `v145` 平台工具集已经可用。
4. 重新运行：

```powershell
.\scripts\build.ps1
```

补充说明：

- 本项目的三个 `.vcxproj` 都显式使用 `PlatformToolset=v145`。
- 仅有 `MSVC` 编译器目录但缺少对应 `PlatformToolsets\v145\Toolset.props` 时，仍然会报这个错误。

## 注册前验证阶段

### 现象：`smoke-test.ps1` 中 `ComSmoke.exe` 报 `0x80040154`

典型报错：

```text
CoCreateInstance failed: 0x80040154
```

原因：

- `.\scripts\smoke-test.ps1` 会先调用 `.\scripts\build.ps1`。
- 然后它会运行 `build\Release\x64\ComSmoke.exe`。
- `ComSmoke.exe` 依赖“当前 HKCU 已存在 CLSID 注册”。
- 如果还没有执行 `.\scripts\register.ps1`，那么 COM 对象尚未注册，报 `0x80040154` 是正常现象。

处理：

1. 先确认构建已经通过。
2. 执行当前用户注册：

```powershell
.\scripts\register.ps1 -RestartExplorer
```

3. 再单独运行：

```powershell
.\build\Release\x64\ComSmoke.exe
```

成功时会输出：

```text
title=Open project in Codex
```

补充说明：

- `smoke-test.ps1` 更适合用于“已注册状态”的验证。
- 如果你只是想验证构建和 staging 产物是否存在，可以先看 `build\Release\x64` 和 `dist\sparse-package`。

## 注册阶段

### 现象：`register.ps1` 中 `Add-AppxPackage` 报 `0x80073CFF`

典型报错：

```text
Deployment failed with HRESULT: 0x80073CFF
```

原因：

- `register.ps1` 会调用：

```powershell
Add-AppxPackage -Register $Manifest -ExternalLocation $StageDir -ForceUpdateFromAnyVersion
```

- 本项目使用的是未签名 sparse package。
- 如果没有启用 Windows 开发者模式，系统会拒绝注册未签名包。

处理：

1. 在 Windows 设置中启用“开发者模式”。
2. 重新运行：

```powershell
.\scripts\register.ps1 -RestartExplorer
```

成功时会看到类似输出：

```text
Registered package: OpenAI.CodexContextMenu_1.0.0.0_x64__p6xwh4jdx6qx8
Context menu registration completed.
```

## 注册后验证

注册成功后，建议至少检查以下项目：

### 包注册

```powershell
Get-AppxPackage -Name OpenAI.CodexContextMenu
```

### 文件夹对象右键菜单

```powershell
reg query HKCU\Software\Classes\Directory\shell\OpenProjectInCodex /s
```

### 文件夹空白处右键菜单

```powershell
reg query HKCU\Software\Classes\Directory\Background\shell\OpenProjectInCodex /s
```

### COM CLSID 注册

```powershell
reg query "HKCU\Software\Classes\CLSID\{7F07B25C-22DE-46D3-9747-6B2D6B07F54D}" /s
```

### COM 冒烟检查

```powershell
.\build\Release\x64\ComSmoke.exe
```

成功时应能看到标题输出：

```text
title=Open project in Codex
```

### 现象：菜单已经出现，但图标不对

典型现象：

- 菜单文本已经能显示。
- 但图标显示成错误图标、空白图标，或者不是预期的 Codex 图标。

原因：

- 经典菜单和 Win11 主菜单都会走 `Icon` 值或 `IExplorerCommand::GetIcon` 的返回值。
- 如果这里直接指向 `codex.exe`，资源管理器未必会稳定解析出预期图标。
- 本次验证里，更稳定的做法是使用项目自带图标资源，在 staging 目录生成独立的 `codex-context-menu.ico`，然后统一让注册表和 COM 菜单读取它。

处理：

1. 确认 staging 目录里存在：

```powershell
Get-Item .\dist\sparse-package\codex-context-menu.ico
```

2. 确认两个菜单键的 `Icon` 已经指向这个 `.ico` 文件：

```powershell
reg query HKCU\Software\Classes\Directory\shell\OpenProjectInCodex /v Icon
reg query HKCU\Software\Classes\Directory\Background\shell\OpenProjectInCodex /v Icon
```

3. 重新执行注册并重启 Explorer：

```powershell
.\scripts\register.ps1 -RestartExplorer
```

补充说明：

- 当前实现会从 `assets\codex-context-menu.png` 生成 `dist\sparse-package\codex-context-menu.ico`。
- 这是为了避免继续依赖 `codex.exe` 的默认图标解析行为。

### 现象：已注册状态下，`build.ps1` 或 `smoke-test.ps1` 报 `microsoft.system.package.metadata` 路径错误

典型报错：

```text
Could not find a part of the path '...\dist\sparse-package\microsoft.system.package.metadata'
```

原因：

- `register.ps1` 成功后，Windows 会在 `dist\sparse-package` 下维护隐藏目录 `microsoft.system.package.metadata`。
- 如果 `build.ps1` 在下一次构建时直接整目录删除 `dist\sparse-package`，就可能和系统维护中的 sparse package 元数据目录冲突。
- 这会让“已注册状态下重新构建”或“已注册状态下运行 `smoke-test.ps1`”变得不稳定。

处理：

1. 不要在已注册状态下整目录删除 `dist\sparse-package`。
2. 改为保留 `microsoft.system.package.metadata`，只清理并重写当前项目自己生成的文件。
3. 然后重新执行：

```powershell
.\scripts\build.ps1
.\scripts\smoke-test.ps1
```

补充说明：

- 当前仓库已经按这个方式修复：保留 sparse package 根目录和系统元数据目录，只替换 DLL、EXE、manifest、PNG、ICO 等 staging 文件。
- 这个问题属于“已注册后再次构建”的场景，首次构建通常不会触发。

## 卸载验证

执行：

```powershell
.\scripts\unregister.ps1 -RestartExplorer
```

成功后应满足：

- `Get-AppxPackage -Name OpenAI.CodexContextMenu` 无结果
- `HKCU\Software\Classes\Directory\shell\OpenProjectInCodex` 已删除
- `HKCU\Software\Classes\Directory\Background\shell\OpenProjectInCodex` 已删除
- `HKCU\Software\Classes\CLSID\{7F07B25C-22DE-46D3-9747-6B2D6B07F54D}` 已删除

可用命令：

```powershell
Get-AppxPackage -Name OpenAI.CodexContextMenu
reg query HKCU\Software\Classes\Directory\shell\OpenProjectInCodex
reg query HKCU\Software\Classes\Directory\Background\shell\OpenProjectInCodex
reg query "HKCU\Software\Classes\CLSID\{7F07B25C-22DE-46D3-9747-6B2D6B07F54D}"
```

## 推荐验证顺序

推荐按下面顺序执行，能减少误判：

1. 构建：

```powershell
.\scripts\build.ps1
```

2. 构建与 staging 文件检查：

```powershell
.\scripts\smoke-test.ps1
```

说明：

- 如果此时 `ComSmoke.exe` 因未注册而报 `0x80040154`，先不要把它当成构建失败，继续执行注册步骤。

3. 注册当前用户菜单和 sparse package：

```powershell
.\scripts\register.ps1 -RestartExplorer
```

4. 检查包、注册表和 COM：

```powershell
Get-AppxPackage -Name OpenAI.CodexContextMenu
reg query HKCU\Software\Classes\Directory\shell\OpenProjectInCodex /s
reg query HKCU\Software\Classes\Directory\Background\shell\OpenProjectInCodex /s
reg query "HKCU\Software\Classes\CLSID\{7F07B25C-22DE-46D3-9747-6B2D6B07F54D}" /s
.\build\Release\x64\ComSmoke.exe
```

5. 卸载并验证清理：

```powershell
.\scripts\unregister.ps1 -RestartExplorer
```
