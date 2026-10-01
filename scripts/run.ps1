# Launches the built MockTerm executable. Extra arguments are passed through,
# e.g. scripts\run.ps1 --remote-debugging-port=9222
param(
  [ValidateSet('Release', 'Debug')][string]$Config = 'Release',
  [Parameter(ValueFromRemainingArguments = $true)][string[]]$Rest
)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$exe = Join-Path $root "build/host/$Config/mockterm.exe"
if (-not (Test-Path $exe)) { throw "$exe not found; run scripts\build.ps1 first." }
if ($Rest) { Start-Process -FilePath $exe -ArgumentList $Rest -WorkingDirectory (Split-Path $exe) }
else { Start-Process -FilePath $exe -WorkingDirectory (Split-Path $exe) }
