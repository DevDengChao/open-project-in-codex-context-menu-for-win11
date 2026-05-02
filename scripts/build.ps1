[CmdletBinding()]
param(
  [ValidateSet('Debug', 'Release')]
  [string] $Configuration = 'Release',

  [ValidateSet('x64')]
  [string] $Platform = 'x64'
)

$ErrorActionPreference = 'Stop'

$Root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$Solution = Join-Path $Root 'OpenProjectInCodexContextMenu.sln'
$BuildOut = Join-Path $Root "build\$Configuration\$Platform"
$StageDir = Join-Path $Root 'dist\sparse-package'
$PngIcon = Join-Path $Root 'assets\codex-context-menu.png'
$IcoIcon = Join-Path $StageDir 'codex-context-menu.ico'
$PreservedStageEntries = @('microsoft.system.package.metadata')

function Find-MSBuild {
  $candidates = @()

  $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
  if (Test-Path -LiteralPath $vswhere) {
    $installPath = & $vswhere -latest -products * -requires Microsoft.Component.MSBuild -property installationPath
    if ($installPath) {
      $candidates += (Join-Path $installPath 'MSBuild\Current\Bin\amd64\MSBuild.exe')
      $candidates += (Join-Path $installPath 'MSBuild\Current\Bin\MSBuild.exe')
    }
  }

  $candidates += @(
    (Join-Path $env:ProgramFiles 'Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\amd64\MSBuild.exe'),
    (Join-Path $env:ProgramFiles 'Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe'),
    (Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\amd64\MSBuild.exe'),
    (Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe')
  )

  foreach ($candidate in $candidates) {
    if ($candidate -and (Test-Path -LiteralPath $candidate)) {
      return $candidate
    }
  }

  throw 'MSBuild.exe was not found. Install Visual Studio with the Desktop development with C++ workload.'
}

function New-IcoFromPng {
  param(
    [Parameter(Mandatory=$true)][string] $PngPath,
    [Parameter(Mandatory=$true)][string] $IcoPath
  )

  $pngBytes = [System.IO.File]::ReadAllBytes($PngPath)
  $directory = [System.IO.Path]::GetDirectoryName($IcoPath)
  if ($directory) {
    [System.IO.Directory]::CreateDirectory($directory) | Out-Null
  }

  $stream = [System.IO.File]::Open($IcoPath, [System.IO.FileMode]::Create, [System.IO.FileAccess]::Write)
  try {
    $writer = New-Object System.IO.BinaryWriter($stream)
    try {
      $writer.Write([UInt16]0)
      $writer.Write([UInt16]1)
      $writer.Write([UInt16]1)
      $writer.Write([Byte]0)
      $writer.Write([Byte]0)
      $writer.Write([Byte]0)
      $writer.Write([Byte]0)
      $writer.Write([UInt16]1)
      $writer.Write([UInt16]32)
      $writer.Write([UInt32]$pngBytes.Length)
      $writer.Write([UInt32]22)
      $writer.Write($pngBytes)
    } finally {
      $writer.Dispose()
    }
  } finally {
    $stream.Dispose()
  }
}

function Clear-StageDirectory {
  param(
    [Parameter(Mandatory=$true)][string] $Path
  )

  if (!(Test-Path -LiteralPath $Path)) {
    New-Item -ItemType Directory -Force -Path $Path | Out-Null
    return
  }

  Get-ChildItem -LiteralPath $Path -Force | ForEach-Object {
    if ($PreservedStageEntries -contains $_.Name) {
      return
    }
    Remove-Item -LiteralPath $_.FullName -Recurse -Force
  }
}

$msbuild = Find-MSBuild
Write-Host "Using MSBuild: $msbuild"
& $msbuild $Solution /m "/p:Configuration=$Configuration" "/p:Platform=$Platform" /v:minimal

if (!(Test-Path -LiteralPath $BuildOut)) {
  throw "Build output directory was not created: $BuildOut"
}

$stageRoot = [System.IO.Path]::GetFullPath($StageDir)
$projectRoot = [System.IO.Path]::GetFullPath($Root)
if (!$stageRoot.StartsWith($projectRoot, [System.StringComparison]::OrdinalIgnoreCase)) {
  throw "Refusing to stage outside project root: $stageRoot"
}

Clear-StageDirectory -Path $StageDir

Copy-Item -LiteralPath (Join-Path $BuildOut 'CodexContextMenu.dll') -Destination (Join-Path $StageDir 'CodexContextMenu.dll') -Force
Copy-Item -LiteralPath (Join-Path $BuildOut 'CodexContextMenuHost.exe') -Destination (Join-Path $StageDir 'CodexContextMenuHost.exe') -Force
Copy-Item -LiteralPath $PngIcon -Destination (Join-Path $StageDir 'codex-context-menu.png') -Force
New-IcoFromPng -PngPath $PngIcon -IcoPath $IcoIcon
Copy-Item -LiteralPath (Join-Path $Root 'appx\AppxManifest.xml') -Destination (Join-Path $StageDir 'AppxManifest.xml') -Force

Write-Host "Staged sparse package: $StageDir"
