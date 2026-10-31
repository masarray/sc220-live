param(
    [ValidateSet('Release','Debug')]
    [string]$Configuration = 'Release',
    [string]$JuceSourceDir = '',
    [switch]$Clean
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
Push-Location $root
try {
    $expectedBranch = 'feature/asio-input-vst3-and-public-runner-cleanup'
    $head = (& git rev-parse --abbrev-ref HEAD).Trim()
    if ($LASTEXITCODE -ne 0) { throw 'Run this script inside a local SC220 Live Git checkout.' }
    if ($head -ne $expectedBranch) {
        throw "Checkout the development branch first: git fetch origin $expectedBranch; git switch $expectedBranch"
    }

    Write-Host 'SC220 Live ASIO/VST3 HARDWARE PREVIEW - source branch verified' -ForegroundColor Cyan
    & git submodule update --init --recursive third_party/askp-vst
    if ($LASTEXITCODE -ne 0) {
        throw 'Private ASK-P submodule checkout failed. Sign in with Git Credential Manager using an account with read access to masarray/askp-vst.'
    }

    & python qa/audit_sc220_asio_vst3.py
    if ($LASTEXITCODE -ne 0) { throw 'RT source safety guard failed.' }
    & cmake -P qa/verify_effective_cmake_pipeline.cmake
    if ($LASTEXITCODE -ne 0) { throw 'Canonical generated runtime source guard failed.' }

    $buildArgs = @{
        Configuration = $Configuration
        BuildDir = 'build-asio-vst3-hardware-preview'
        Generator = 'Auto'
    }
    if ($JuceSourceDir) { $buildArgs.JuceSourceDir = $JuceSourceDir }
    if ($Clean) { $buildArgs.Clean = $true }
    & "$PSScriptRoot/build-live-windows.ps1" @buildArgs
    if (-not $?) { throw 'Windows build script did not complete successfully.' }

    $dir = Join-Path $root 'build-asio-vst3-hardware-preview'
    $exe = Get-ChildItem $dir -File -Recurse -Filter '*.exe' |
        Where-Object { $_.BaseName -in @('SC220 Live', 'SC220Live') } |
        Select-Object -First 1
    if (-not $exe) { throw 'Build completed but SC220 Live EXE not found.' }

    Write-Host ''
    Write-Host 'WINDOWS PREVIEW COMPILED - READY FOR PHYSICAL ASIO/VST3 TEST' -ForegroundColor Green
    Write-Host "EXE: $($exe.FullName)"
    Write-Host "SHA256: $((Get-FileHash -Path $exe.FullName -Algorithm SHA256).Hash)"
    Write-Host '1. Open SC220 Live > Settings > Audio Setup: select your native ASIO driver (or WASAPI).'
    Write-Host '2. Use the same driver for ASIO input/output; do not silently route across device clocks.'
    Write-Host '3. Start at 48 kHz / 256 samples, then test 128 and 64.'
    Write-Host '4. Input > + VST3 FX: select trusted ArSonVerb.vst3, open editor, bypass and remove.'
    Write-Host '5. Test PC Audio, physical monitor, Noise Learn and 500 preset/bypass operations.'
    Write-Host '6. Watch callback deadline misses, FIFO underflows and audible crackles.'
    Write-Host 'This unsigned EXE is a development preview, not the official public release.'
} finally {
    Pop-Location
}
