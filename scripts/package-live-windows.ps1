param(
    [ValidateSet('Release','Debug')]
    [string]$Configuration = 'Release',
    [string]$BuildDir = 'build-live',
    [string]$Version = '0.13.0',
    [string]$JuceSourceDir = '',
    [switch]$Clean
)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path

function Remove-PathWithRetry([string]$path) {
    if (-not (Test-Path $path)) { return }
    for ($attempt = 1; $attempt -le 6; $attempt++) {
        try {
            Remove-Item -LiteralPath $path -Recurse -Force -ErrorAction Stop
            return
        } catch {
            if ($attempt -eq 6) { throw }
            Start-Sleep -Milliseconds (300 * $attempt)
        }
    }
}

$buildScriptArgs = @{
    Configuration = $Configuration
    BuildDir = $BuildDir
    JuceSourceDir = $JuceSourceDir
}
if ($Clean) { $buildScriptArgs.Clean = $true }

& (Join-Path $PSScriptRoot 'build-live-windows.ps1') @buildScriptArgs

$build = Join-Path $root $BuildDir
$exe = Get-ChildItem $build -Recurse -Filter '*.exe' |
    Where-Object { $_.Name -match '(Sonkupik.*Live)' } |
    Select-Object -First 1
if (-not $exe) { throw 'Sonkupik Live executable was not found after build.' }

$distRoot = Join-Path $root 'dist'
$packageName = "Sonkupik-Live-v$Version-win-x64-portable"
$stage = Join-Path $distRoot $packageName
$zip = Join-Path $distRoot "$packageName.zip"

Remove-PathWithRetry $stage
Remove-PathWithRetry $zip
New-Item -ItemType Directory -Force -Path $stage | Out-Null

Copy-Item $exe.FullName (Join-Path $stage 'Sonkupik Live.exe')
Copy-Item (Join-Path $root 'THIRD_PARTY_NOTICES.md') $stage
Copy-Item (Join-Path $root 'docs\BUILD_WINDOWS.md') $stage
Copy-Item (Join-Path $root 'docs\PRODUCT_STRATEGY.md') $stage

@'
SONKUPIK LIVE - PORTABLE PREVIEW

1. Install the standard VB-Audio VB-CABLE separately from the official source.
2. Start Sonkupik Live as a normal desktop application.
3. Choose your audio interface as INPUT.
4. Choose CABLE Input (VB-Audio Virtual Cable) as OUTPUT.
5. In OBS/TikTok Live Studio choose CABLE Output as the audio source.
6. Press Learn Noise while only static source noise is present.

This preview does not silently install VB-CABLE and is not code-signed.
'@ | Set-Content -Encoding UTF8 (Join-Path $stage 'QUICK_START.txt')

Compress-Archive -Path (Join-Path $stage '*') -DestinationPath $zip -CompressionLevel Optimal
Write-Host "PACKAGE: $zip" -ForegroundColor Green
