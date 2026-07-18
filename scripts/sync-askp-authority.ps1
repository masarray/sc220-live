param(
    [string]$Destination = '',
    [string]$Repository = 'https://github.com/masarray/askp-vst.git',
    [string]$Commit = '655874dee6ee69df7212b8073ab941f9607d8faa',
    [switch]$Force
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version 2.0

$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
if (-not $Destination) { $Destination = Join-Path $root '.deps\askp-vst' }
$Destination = [IO.Path]::GetFullPath($Destination)

if (-not (Get-Command git.exe -ErrorAction SilentlyContinue) -and
    -not (Get-Command git -ErrorAction SilentlyContinue)) {
    throw 'Git was not found. Install Git for Windows first.'
}

if ((Test-Path $Destination) -and $Force) {
    Write-Host "==> Removing $Destination"
    Remove-Item -Recurse -Force $Destination
}

if (-not (Test-Path (Join-Path $Destination '.git'))) {
    New-Item -ItemType Directory -Force -Path (Split-Path $Destination -Parent) | Out-Null
    Write-Host "==> Cloning private ArSonKuPik source to $Destination"
    Write-Host '    Git Credential Manager or GitHub CLI may request authentication.'
    git clone --filter=blob:none --no-checkout $Repository $Destination
    if ($LASTEXITCODE -ne 0) { throw "git clone failed with exit code $LASTEXITCODE" }
}

Write-Host "==> Fetching audited authority commit $Commit"
git -C $Destination fetch --depth=1 origin $Commit
if ($LASTEXITCODE -ne 0) { throw "git fetch failed with exit code $LASTEXITCODE" }
git -C $Destination checkout --detach $Commit
if ($LASTEXITCODE -ne 0) { throw "git checkout failed with exit code $LASTEXITCODE" }

$required = @(
    'engine\arsonkupik_dsp.cpp',
    'engine\arsonkupik_dsp.hpp',
    'engine\arsonkupik_presets.cpp',
    'src\OrbVisualizer.cpp',
    'src\OrbVisualizer.h'
)
foreach ($relative in $required) {
    if (-not (Test-Path (Join-Path $Destination $relative))) {
        throw "Authority checkout is incomplete: missing $relative"
    }
}

$actual = (git -C $Destination rev-parse HEAD).Trim()
if ($actual -ne $Commit) { throw "Authority lock mismatch: expected $Commit, got $actual" }

Write-Host "`nSUCCESS: ArSonKuPik authority ready at $Destination" -ForegroundColor Green
Write-Host "Build with: .\scripts\build-live-windows.ps1 -AskpSourceDir `"$Destination`" -Clean"
