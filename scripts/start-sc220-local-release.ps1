param(
    [string]$Repository = 'masarray/sc220-live',
    [string]$DownloadRepository = 'masarray/sc220-download',
    [string]$Version = '',
    [string]$SourceRef = '',
    [ValidateSet('stable', 'prerelease')]
    [string]$Channel = 'stable',
    [string]$ReleaseNotes = '',
    [switch]$NoWatch
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version 2.0

function Write-Section([string]$Title) {
    Write-Host "`n============================================================" -ForegroundColor DarkCyan
    Write-Host " $Title" -ForegroundColor Cyan
    Write-Host "============================================================" -ForegroundColor DarkCyan
}

function Invoke-Gh {
    param([Parameter(ValueFromRemainingArguments = $true)][string[]]$Arguments)
    & gh.exe @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "GitHub CLI failed: gh $($Arguments -join ' ')"
    }
}

function Get-GhJson {
    param([Parameter(ValueFromRemainingArguments = $true)][string[]]$Arguments)
    $text = (& gh.exe @Arguments | Out-String)
    if ($LASTEXITCODE -ne 0) {
        throw "GitHub CLI failed: gh $($Arguments -join ' ')"
    }
    if ([string]::IsNullOrWhiteSpace($text)) { return $null }
    return $text | ConvertFrom-Json
}

function Get-NextPatchVersion([string]$Current) {
    if ($Current -notmatch '^(\d+)\.(\d+)\.(\d+)$') { return '0.2.0' }
    return '{0}.{1}.{2}' -f [int]$Matches[1], [int]$Matches[2], ([int]$Matches[3] + 1)
}

function Test-RunnerOnline {
    try {
        $runnerData = Get-GhJson api "repos/$Repository/actions/runners?per_page=100"
        $matching = @($runnerData.runners) | Where-Object {
            $_.status -eq 'online' -and
            (@($_.labels | ForEach-Object { $_.name }) -contains 'sc220-build')
        }
        return $matching | Select-Object -First 1
    }
    catch {
        Write-Warning 'Runner status could not be queried. The workflow can still be queued, but GitHub login may need repo administration access.'
        return $null
    }
}

$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
Set-Location $root

Write-Section 'SC220 Live - local build and automatic public release'

if (-not (Get-Command git.exe -ErrorAction SilentlyContinue)) {
    throw 'Git is missing. Run SETUP-LOCAL-RUNNER.cmd first.'
}
if (-not (Get-Command gh.exe -ErrorAction SilentlyContinue)) {
    throw 'GitHub CLI is missing. Run SETUP-LOCAL-RUNNER.cmd first.'
}

& gh.exe auth status -h github.com *> $null
if ($LASTEXITCODE -ne 0) {
    Write-Host 'GitHub login is required. A browser window will open.'
    Invoke-Gh auth login --hostname github.com --git-protocol https --web --scopes 'repo,workflow'
}

$insideRepo = (& git.exe rev-parse --is-inside-work-tree 2>$null | Select-Object -First 1)
if ($insideRepo -ne 'true') {
    throw 'RELEASE-SC220.cmd must be run from a cloned sc220-live repository.'
}

$dirty = (& git.exe status --porcelain | Out-String).Trim()
if ($dirty) {
    throw @"
Local files are not clean. Commit and push all intended changes first.
The release workflow builds a GitHub ref, not uncommitted files on this PC.

$dirty
"@
}

$currentBranch = (& git.exe branch --show-current | Select-Object -First 1).Trim()
if (-not $SourceRef) {
    $SourceRef = if ($currentBranch) { $currentBranch } else { 'main' }
}

Write-Host "Checking remote source ref: $SourceRef"
& git.exe fetch origin $SourceRef --quiet
if ($LASTEXITCODE -ne 0) {
    throw "Remote source ref origin/$SourceRef was not found. Push the branch before releasing."
}

if ($currentBranch -eq $SourceRef) {
    $localHead = (& git.exe rev-parse HEAD | Select-Object -First 1).Trim()
    $remoteHead = (& git.exe rev-parse "origin/$SourceRef" | Select-Object -First 1).Trim()
    if ($localHead -ne $remoteHead) {
        throw "Local HEAD ($localHead) is not the pushed origin/$SourceRef ($remoteHead). Push or pull before releasing."
    }
}

$currentRelease = $null
try {
    $currentRelease = Invoke-RestMethod -UseBasicParsing `
        -Uri 'https://raw.githubusercontent.com/masarray/sc220-download/main/latest.json' `
        -Headers @{ 'Cache-Control' = 'no-cache'; 'User-Agent' = 'SC220-Local-Release/1.0' }
}
catch {
    Write-Warning 'Could not read sc220-download/latest.json; using a conservative version suggestion.'
}

if (-not $Version) {
    $currentVersion = if ($currentRelease -and $currentRelease.version) {
        [string]$currentRelease.version
    } else {
        '0.1.0'
    }
    $suggested = Get-NextPatchVersion $currentVersion
    $entered = Read-Host "Release version without v [default $suggested]"
    $Version = if ([string]::IsNullOrWhiteSpace($entered)) { $suggested } else { $entered.Trim() }
}

if ($Version -notmatch '^\d+\.\d+\.\d+$') {
    throw "Invalid semantic version: $Version"
}

if (-not $ReleaseNotes) {
    $ReleaseNotes = Read-Host 'One-line release highlight [optional]'
}

$channelAnswer = Read-Host "Release channel stable/prerelease [default $Channel]"
if (-not [string]::IsNullOrWhiteSpace($channelAnswer)) {
    if ($channelAnswer -notin @('stable', 'prerelease')) {
        throw "Unsupported release channel: $channelAnswer"
    }
    $Channel = $channelAnswer
}

$runner = Test-RunnerOnline
if ($runner) {
    Write-Host "Runner online : $($runner.name)" -ForegroundColor Green
}
else {
    Write-Warning 'No online runner with label sc220-build was confirmed.'
    $continue = Read-Host 'Queue the workflow anyway? [y/N]'
    if ($continue -notmatch '^(?i)y(?:es)?$') {
        throw 'Release cancelled. Run SETUP-LOCAL-RUNNER.cmd or start the runner service.'
    }
}

Write-Section 'Release confirmation'
Write-Host "Source       : $Repository@$SourceRef"
Write-Host "Version      : v$Version"
Write-Host "Channel      : $Channel"
Write-Host "Destination  : $DownloadRepository"
Write-Host "Build runner : local PC (self-hosted)"
Write-Host "Notes        : $ReleaseNotes"
Write-Host ''
$confirm = Read-Host 'Type RELEASE to build, test, and publish'
if ($confirm -cne 'RELEASE') {
    throw 'Release cancelled by user.'
}

$workflowRef = $SourceRef
$dispatchStarted = [DateTime]::UtcNow
$workflowArguments = @(
    'workflow', 'run', 'local-release.yml',
    '--repo', $Repository,
    '--ref', $workflowRef,
    '-f', "version=$Version",
    '-f', "source_ref=$SourceRef",
    '-f', 'clean=true',
    '-f', "release_channel=$Channel",
    '-f', "release_notes=$ReleaseNotes"
)
Invoke-Gh @workflowArguments
Write-Host 'Workflow queued. Waiting for GitHub to assign a run ID...'

$run = $null
for ($attempt = 0; $attempt -lt 20 -and -not $run; $attempt++) {
    Start-Sleep -Seconds 2
    $runs = @(Get-GhJson run list `
        --repo $Repository `
        --workflow 'local-release.yml' `
        --branch $workflowRef `
        --event workflow_dispatch `
        --limit 10 `
        --json 'databaseId,createdAt,status,url,headBranch,displayTitle')

    $run = $runs | Where-Object {
        ([DateTime]$_.createdAt).ToUniversalTime() -ge $dispatchStarted.AddSeconds(-5)
    } | Sort-Object { [DateTime]$_.createdAt } -Descending | Select-Object -First 1
}

if (-not $run) {
    throw 'Workflow was dispatched, but its run ID could not be located. Check the Actions tab.'
}

Write-Host "Run URL: $($run.url)" -ForegroundColor Cyan
if (-not $NoWatch) {
    & gh.exe run watch $run.databaseId --repo $Repository --exit-status
    if ($LASTEXITCODE -ne 0) {
        throw "Local release workflow failed. Open $($run.url) for details."
    }
}

$releaseUrl = "https://github.com/$DownloadRepository/releases/tag/v$Version"
Write-Section 'RELEASE BERHASIL'
Write-Host $releaseUrl -ForegroundColor Green
Start-Process $releaseUrl
