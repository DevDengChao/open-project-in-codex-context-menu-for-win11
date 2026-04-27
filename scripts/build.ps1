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

if (Test-Path -LiteralPath $StageDir) {
  Remove-Item -LiteralPath $StageDir -Recurse -Force
}
New-Item -ItemType Directory -Force -Path $StageDir | Out-Null

Copy-Item -LiteralPath (Join-Path $BuildOut 'CodexContextMenu.dll') -Destination (Join-Path $StageDir 'CodexContextMenu.dll') -Force
Copy-Item -LiteralPath (Join-Path $BuildOut 'CodexContextMenuHost.exe') -Destination (Join-Path $StageDir 'CodexContextMenuHost.exe') -Force
Copy-Item -LiteralPath (Join-Path $Root 'assets\codex-context-menu.png') -Destination (Join-Path $StageDir 'codex-context-menu.png') -Force
Copy-Item -LiteralPath (Join-Path $Root 'appx\AppxManifest.xml') -Destination (Join-Path $StageDir 'AppxManifest.xml') -Force

Write-Host "Staged sparse package: $StageDir"
