[CmdletBinding()]
param(
  [ValidateSet('Debug', 'Release')]
  [string] $Configuration = 'Release',

  [ValidateSet('x64')]
  [string] $Platform = 'x64',

  [switch] $RestartExplorer
)

$ErrorActionPreference = 'Stop'

$Root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$StageDir = Join-Path $Root 'dist\sparse-package'
$Manifest = Join-Path $StageDir 'AppxManifest.xml'
$Dll = Join-Path $StageDir 'CodexContextMenu.dll'
$HostExe = Join-Path $StageDir 'CodexContextMenuHost.exe'
$MenuIcon = Join-Path $StageDir 'codex-context-menu.ico'
$Clsid = '{7F07B25C-22DE-46D3-9747-6B2D6B07F54D}'
$PackageName = 'OpenAI.CodexContextMenu'

& (Join-Path $PSScriptRoot 'build.ps1') -Configuration $Configuration -Platform $Platform

if (!(Test-Path -LiteralPath $Manifest)) {
  throw "Sparse package manifest was not staged: $Manifest"
}
if (!(Test-Path -LiteralPath $Dll)) {
  throw "Shell extension DLL was not staged: $Dll"
}
if (!(Test-Path -LiteralPath $HostExe)) {
  throw "Context menu host EXE was not staged: $HostExe"
}
if (!(Test-Path -LiteralPath $MenuIcon)) {
  throw "Context menu icon was not staged: $MenuIcon"
}

function Set-DefaultValue {
  param(
    [Parameter(Mandatory=$true)][string] $Path,
    [Parameter(Mandatory=$true)][string] $Value
  )

  New-Item -Path $Path -Force | Out-Null
  Set-Item -Path $Path -Value $Value
}

function Set-ContextMenuKey {
  param(
    [Parameter(Mandatory=$true)][string] $BasePath,
    [Parameter(Mandatory=$true)][string] $Placeholder,
    [Parameter(Mandatory=$true)][string] $IconPath
  )

  $commandPath = Join-Path $BasePath 'command'
  Set-DefaultValue -Path $BasePath -Value 'Open project in Codex'
  New-ItemProperty -Path $BasePath -Name 'Icon' -PropertyType String -Value $IconPath -Force | Out-Null
  Set-DefaultValue -Path $commandPath -Value "`"$HostExe`" `"$Placeholder`""
}

Set-ContextMenuKey -BasePath 'HKCU:\Software\Classes\Directory\shell\OpenProjectInCodex' -Placeholder '%1' -IconPath $MenuIcon
Set-ContextMenuKey -BasePath 'HKCU:\Software\Classes\Directory\Background\shell\OpenProjectInCodex' -Placeholder '%V' -IconPath $MenuIcon

$clsidPath = "HKCU:\Software\Classes\CLSID\$Clsid"
$serverPath = Join-Path $clsidPath 'InProcServer32'
Set-DefaultValue -Path $clsidPath -Value 'Codex Context Menu'
Set-DefaultValue -Path $serverPath -Value $Dll
New-ItemProperty -Path $serverPath -Name 'ThreadingModel' -PropertyType String -Value 'Apartment' -Force | Out-Null

Write-Host "Registering sparse package from: $StageDir"
Add-AppxPackage -Register $Manifest -ExternalLocation $StageDir -ForceUpdateFromAnyVersion

$package = Get-AppxPackage -Name $PackageName -ErrorAction SilentlyContinue
if (!$package) {
  throw "Package was not registered: $PackageName"
}

Write-Host "Registered package: $($package.PackageFullName)"

if ($RestartExplorer) {
  Stop-Process -Name explorer -Force
  Start-Process explorer.exe
}

Write-Host 'Context menu registration completed.'
