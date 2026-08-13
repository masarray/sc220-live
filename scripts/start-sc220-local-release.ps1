param(
    [ValidatePattern('^$|^\d+\.\d+\.\d+$')]
    [string]$Version = '',
    [switch]$Clean,
    [ValidateSet('stable', 'prerelease')]
    [string]$Channel = 'stable',
    [string]$ReleaseNotes = '',
    [switch]$Yes
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version 3.0

$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
Set-Location $root

foreach ($command in @('git.exe', 'gh.exe', 'python.exe')) {
    if (-not (Get-Command $command -ErrorAction SilentlyContinue)) {
        throw "$command is required for the local release handoff."
    }
}

if (-not $Version) {
    $projectLine = Select-String -Path (Join-Path $root 'CMakeLists.txt') `
        -Pattern '^project\(SC220Live VERSION ([0-9]+\.[0-9]+\.[0-9]+)' |
        Select-Object -First 1
    if (-not $projectLine) { throw 'Unable to derive SC220 version from CMakeLists.txt.' }
    $Version = $projectLine.Matches[0].Groups[1].Value
}

$branch = (& git.exe branch --show-current).Trim()
if ($branch -ne 'main') { throw "Release requires main; current branch is '$branch'." }
if (& git.exe status --porcelain) { throw 'Release requires a clean local checkout.' }

& git.exe fetch origin main --quiet
if ($LASTEXITCODE -ne 0) { throw 'Unable to fetch private origin/main.' }
$sourceCommit = (& git.exe rev-parse HEAD).Trim()
$remoteCommit = (& git.exe rev-parse origin/main).Trim()
if ($sourceCommit -ne $remoteCommit) {
    throw "Release source must equal origin/main. HEAD=$sourceCommit origin/main=$remoteCommit"
}

$publicRepo = & gh.exe repo view masarray/sc220-download --json isPrivate --jq .isPrivate
if ($LASTEXITCODE -ne 0 -or $publicRepo.Trim() -ne 'false') {
    throw 'masarray/sc220-download must be reachable and public.'
}

if (-not $Yes) {
    Write-Host "SC220 Live v$Version" -ForegroundColor Cyan
    Write-Host "Source : $sourceCommit"
    Write-Host 'QA/build/package: local Windows workstation'
    Write-Host 'Publication     : binary-only draft -> public sc220-download workflow'
    $confirmation = Read-Host 'Type RELEASE to continue'
    if (-not $confirmation.Equals('RELEASE', [StringComparison]::OrdinalIgnoreCase)) {
        throw 'Release cancelled by user.'
    }
}

& (Join-Path $PSScriptRoot 'run-sc220-local-qa.ps1') `
    -Configuration Release `
    -BuildDir build-sc220 `
    -Clean:$Clean

$vbLock = Get-Content -LiteralPath (Join-Path $root 'VB_CABLE_AUTHORITY.lock')
$vbHashLine = $vbLock | Where-Object { $_ -match '^package_sha256=' } | Select-Object -First 1
if (-not $vbHashLine) { throw 'VB_CABLE_AUTHORITY.lock has no package_sha256.' }
$vbHash = ($vbHashLine -split '=', 2)[1].Trim()

& (Join-Path $PSScriptRoot 'package-sc220-installer.ps1') `
    -Version $Version `
    -VBCableSha256 $vbHash `
    -SkipBuild

& (Join-Path $PSScriptRoot 'publish-sc220-download.ps1') `
    -Version $Version `
    -SourceCommit $sourceCommit `
    -Channel $Channel `
    -ReleaseNotes $ReleaseNotes

Write-Host "`nSC220 RELEASE VERIFIED" -ForegroundColor Green
Write-Host "https://github.com/masarray/sc220-download/releases/tag/v$Version"
