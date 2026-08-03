param(
    [ValidateSet('Release','Debug')]
    [string]$Configuration = 'Release',
    [switch]$Clean,
    [switch]$NoExplorer
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version 2.0

$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
Set-Location $root

if (-not (Get-Command git.exe -ErrorAction SilentlyContinue)) {
    throw 'Git for Windows is required. Install Git and run this file again.'
}

if (-not (Test-Path (Join-Path $root 'third_party\askp-vst\engine\arsonkupik_dsp.cpp'))) {
    Write-Host '==> Initialising the pinned ASK-P DSP submodule'
    & git submodule update --init --recursive -- third_party/askp-vst
    if ($LASTEXITCODE -ne 0) {
        throw 'ASK-P DSP submodule could not be initialised. Check access to masarray/askp-vst.'
    }
}

& (Join-Path $PSScriptRoot 'check-sc220-release-contract.ps1')

$arguments = @{
    Configuration = $Configuration
    BuildDir = 'build-sc220'
    Generator = 'Ninja'
}
if ($Clean) { $arguments.Clean = $true }

& (Join-Path $PSScriptRoot 'build-live-windows.ps1') @arguments

$exe = Join-Path $root "build-sc220\SC220Live_artefacts\$Configuration\SC220 Live.exe"
if (-not (Test-Path $exe)) {
    $exe = Get-ChildItem (Join-Path $root 'build-sc220') -Recurse -Filter 'SC220 Live.exe' |
        Select-Object -First 1 -ExpandProperty FullName
}
if (-not $exe -or -not (Test-Path $exe)) {
    throw 'Compiler finished without the expected SC220 Live executable.'
}

Write-Host "`nSC220 LIVE BUILD OK" -ForegroundColor Green
Write-Host "Executable: $exe"
if (-not $NoExplorer) {
    Start-Process explorer.exe -ArgumentList "/select,`"$exe`""
}
