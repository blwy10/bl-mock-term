# Builds MockTerm (Release x64): fetches CEF if missing, bundles the TypeScript
# apps, configures/builds with CMake + MSVC, and stages the runtime beside the exe.
# Usage: scripts\build.ps1 [-Config Release|Debug] [-Clean] [-SkipTests]
param(
  [ValidateSet('Release', 'Debug')][string]$Config = 'Release',
  [switch]$Clean,
  [switch]$SkipTests
)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$buildDir = Join-Path $root 'build'
$sw = [Diagnostics.Stopwatch]::StartNew()

function Step($msg) { Write-Host "==> $msg" -ForegroundColor Yellow }
function Invoke-Checked($exe, [string[]]$argv) {
  & $exe @argv
  if ($LASTEXITCODE -ne 0) { throw "$exe $($argv -join ' ') failed ($LASTEXITCODE)" }
}

function Find-CMake {
  $c = Get-Command cmake.exe -ErrorAction SilentlyContinue
  if ($c) { return $c.Source }
  $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
  if (Test-Path $vswhere) {
    $vs = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if ($vs) {
      $bundled = Join-Path $vs 'Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe'
      if (Test-Path $bundled) { return $bundled }
    }
  }
  throw 'cmake.exe not found. Install CMake 3.21+ or the Visual Studio 2022 C++ workload.'
}

Step 'CEF'
& (Join-Path $PSScriptRoot 'get-cef.ps1')

Step 'TypeScript apps'
Push-Location (Join-Path $root 'apps')
try {
  if (-not (Test-Path 'node_modules')) { Invoke-Checked 'npm.cmd' @('ci') }
  Invoke-Checked 'npm.cmd' @('run', 'typecheck')
  if (-not $SkipTests) { Invoke-Checked 'npm.cmd' @('test') }
  Invoke-Checked 'npm.cmd' @('run', 'build')
} finally { Pop-Location }

$cmake = Find-CMake
if ($Clean -and (Test-Path $buildDir)) { Step 'Clean'; Remove-Item -Recurse -Force $buildDir }

Step 'CMake configure'
Invoke-Checked $cmake @('-S', $root, '-B', $buildDir, '-G', 'Visual Studio 17 2022', '-A', 'x64')

Step "CMake build ($Config)"
Invoke-Checked $cmake @('--build', $buildDir, '--config', $Config, '--parallel')

if (-not $SkipTests) {
  Step 'ctest'
  $ctest = Join-Path (Split-Path -Parent $cmake) 'ctest.exe'
  Invoke-Checked $ctest @('--test-dir', $buildDir, '-C', $Config, '--output-on-failure')
}

$exe = Join-Path $buildDir "host/$Config/mockterm.exe"
Write-Host ("Built {0} in {1:mm\:ss}" -f $exe, $sw.Elapsed) -ForegroundColor Green
