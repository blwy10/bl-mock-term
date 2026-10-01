# Downloads and extracts the CEF Standard Distribution pinned in cmake/cef.cmake.
# Usage: scripts\get-cef.ps1 [-Force]
param([switch]$Force)
$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'

$root = Split-Path -Parent $PSScriptRoot
$pin = Get-Content (Join-Path $root 'cmake/cef.cmake') -Raw
if ($pin -notmatch 'set\(CEF_VERSION "([^"]+)"\)') { throw 'CEF_VERSION not found in cmake/cef.cmake' }
$version = $Matches[1]
if ($pin -notmatch 'set\(CEF_PLATFORM "([^"]+)"\)') { throw 'CEF_PLATFORM not found in cmake/cef.cmake' }
$platform = $Matches[1]

$name = "cef_binary_${version}_${platform}"
$cefDir = Join-Path $root 'third_party/cef'
$dest = Join-Path $cefDir $name
if ((Test-Path (Join-Path $dest 'cmake/FindCEF.cmake')) -and -not $Force) {
  Write-Host "CEF $version already present at $dest"
  return
}
New-Item -ItemType Directory -Force -Path $cefDir | Out-Null

$archive = Join-Path $cefDir "$name.tar.bz2"
$url = 'https://cef-builds.spotifycdn.com/' + [Uri]::EscapeDataString("$name.tar.bz2")

# Look up the expected SHA1 from the CEF build index.
$index = Invoke-RestMethod 'https://cef-builds.spotifycdn.com/index.json'
$entry = $index.$platform.versions | Where-Object { $_.cef_version -eq $version } | Select-Object -First 1
if (-not $entry) { throw "CEF $version not found in the CEF build index for $platform" }
$file = $entry.files | Where-Object { $_.type -eq 'standard' } | Select-Object -First 1
$expectedSha1 = $file.sha1

function Get-Sha1($path) { (Get-FileHash -Algorithm SHA1 $path).Hash.ToLowerInvariant() }

if (-not (Test-Path $archive) -or (Get-Sha1 $archive) -ne $expectedSha1) {
  Write-Host "Downloading $url"
  & curl.exe -fSL --retry 3 -o $archive $url
  if ($LASTEXITCODE -ne 0) { throw "Download failed ($LASTEXITCODE)" }
}
$actual = Get-Sha1 $archive
if ($actual -ne $expectedSha1) { throw "SHA1 mismatch for $archive (expected $expectedSha1, got $actual)" }

Write-Host "Extracting $archive"
if (Test-Path $dest) { Remove-Item -Recurse -Force $dest }
# Windows 10+/Server 2019+ ship bsdtar as tar.exe, which handles .tar.bz2.
& "$env:SystemRoot\System32\tar.exe" -xjf $archive -C $cefDir
if ($LASTEXITCODE -ne 0) { throw "Extraction failed ($LASTEXITCODE)" }
Write-Host "CEF $version ready at $dest"
