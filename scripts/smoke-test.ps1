[CmdletBinding()]
param(
  [ValidateSet('Debug', 'Release')]
  [string] $Configuration = 'Release',

  [ValidateSet('x64')]
  [string] $Platform = 'x64'
)

$ErrorActionPreference = 'Stop'

$Root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
& (Join-Path $PSScriptRoot 'build.ps1') -Configuration $Configuration -Platform $Platform

$stage = Join-Path $Root 'dist\sparse-package'
$required = @(
  'AppxManifest.xml',
  'CodexContextMenu.dll',
  'CodexContextMenuHost.exe',
  'codex-context-menu.png'
)

foreach ($file in $required) {
  $path = Join-Path $stage $file
  if (!(Test-Path -LiteralPath $path)) {
    throw "Missing staged file: $path"
  }
}

$testExe = Join-Path $Root "build\$Configuration\$Platform\ComSmoke.exe"
if (Test-Path -LiteralPath $testExe) {
  Write-Host 'Running COM smoke test. This uses the current HKCU CLSID registration.'
  & $testExe
  if ($LASTEXITCODE -ne 0) {
    throw "ComSmoke.exe failed with exit code $LASTEXITCODE"
  }
}

Write-Host 'Smoke test completed.'
