[CmdletBinding()]
param(
  [string] $CodexCli,

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
$MenuIcon = Join-Path $StageDir 'codex-context-menu.ico'
$Clsid = '{7F07B25C-22DE-46D3-9747-6B2D6B07F54D}'
$PackageName = 'OpenAI.CodexContextMenu'

function Find-CodexCli {
  if ($CodexCli) {
    $resolved = Resolve-Path -LiteralPath $CodexCli -ErrorAction SilentlyContinue
    if ($resolved) {
      return $resolved.Path
    }
    throw "Codex CLI was not found: $CodexCli"
  }

  if ($env:CODEX_CLI) {
    $resolved = Resolve-Path -LiteralPath $env:CODEX_CLI -ErrorAction SilentlyContinue
    if ($resolved) {
      return $resolved.Path
    }
  }

  $fromPath = Get-Command codex.exe -ErrorAction SilentlyContinue
  if ($fromPath) {
    return $fromPath.Source
  }

  $packageRoot = Join-Path $env:LOCALAPPDATA 'Packages'
  if (Test-Path -LiteralPath $packageRoot) {
    $cachedCli = Get-ChildItem -LiteralPath $packageRoot -Directory -Filter 'OpenAI.Codex_*' -ErrorAction SilentlyContinue |
      ForEach-Object {
        Join-Path $_.FullName 'LocalCache\Local\OpenAI\Codex\bin\codex.exe'
      } |
      Where-Object {
        Test-Path -LiteralPath $_
      } |
      Select-Object -First 1

    if ($cachedCli) {
      return $cachedCli
    }
  }

  throw 'Codex CLI was not found. Set CODEX_CLI or pass -CodexCli.'
}

$CodexCli = Find-CodexCli

if (!(Test-Path -LiteralPath $CodexCli)) {
  throw "Codex CLI was not found: $CodexCli"
}

& (Join-Path $PSScriptRoot 'build.ps1') -Configuration $Configuration -Platform $Platform

if (!(Test-Path -LiteralPath $Manifest)) {
  throw "Sparse package manifest was not staged: $Manifest"
}
if (!(Test-Path -LiteralPath $Dll)) {
  throw "Shell extension DLL was not staged: $Dll"
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
  Set-DefaultValue -Path $commandPath -Value "`"$CodexCli`" app `"$Placeholder`""
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
