[CmdletBinding()]
param(
  [switch] $RestartExplorer
)

$ErrorActionPreference = 'Stop'

$Clsid = '{7F07B25C-22DE-46D3-9747-6B2D6B07F54D}'
$PackageName = 'OpenAI.CodexContextMenu'

Get-AppxPackage -Name $PackageName -ErrorAction SilentlyContinue | ForEach-Object {
  Write-Host "Removing package: $($_.PackageFullName)"
  Remove-AppxPackage -Package $_.PackageFullName
}

$paths = @(
  'HKCU:\Software\Classes\Directory\shell\OpenProjectInCodex',
  'HKCU:\Software\Classes\Directory\Background\shell\OpenProjectInCodex',
  "HKCU:\Software\Classes\CLSID\$Clsid"
)

foreach ($path in $paths) {
  if (Test-Path -LiteralPath $path) {
    Remove-Item -LiteralPath $path -Recurse -Force
    Write-Host "Removed: $path"
  }
}

if ($RestartExplorer) {
  Stop-Process -Name explorer -Force
  Start-Process explorer.exe
}

Write-Host 'Context menu unregistration completed.'
