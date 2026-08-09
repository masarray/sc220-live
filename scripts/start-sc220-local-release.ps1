param(
    [string]$Repository = 'masarray/sc220-live',
    [string]$DownloadRepository = 'masarray/sc220-download',
    [string]$WorkflowRef = 'main',
    [string]$Version = '',
    [string]$SourceRef = '',
    [ValidateSet('stable', 'prerelease')]
    [string]$Channel = 'stable',
    [string]$ReleaseNotes = ''
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

function Invoke-GhCapture {
    param([Parameter(ValueFromRemainingArguments = $true)][string[]]$Arguments)

    $lines = @(& gh.exe @Arguments 2>&1)
    $exitCode = $LASTEXITCODE
    foreach ($line in $lines) {
        Write-Host ([string]$line)
    }
    if ($exitCode -ne 0) {
        throw "GitHub CLI failed: gh $($Arguments -join ' ')"
    }
    return $lines
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

function Get-LocalRunnerService {
    return Get-Service -Name 'actions.runner.*' -ErrorAction SilentlyContinue |
        Where-Object { $_.Name -match '(?i)sc220|masarray' } |
        Select-Object -First 1
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
        Write-Warning 'Runner status could not be queried. GitHub login may need repository administration access.'
        return $null
    }
}

function Start-And-WaitForRunner([System.ServiceProcess.ServiceController]$Service) {
    if ($Service.Status -ne 'Running') {
        Write-Host "Starting local runner service: $($Service.Name)"
        Start-Service -Name $Service.Name
        $Service = Get-Service -Name $Service.Name
        $Service.WaitForStatus('Running', [TimeSpan]::FromSeconds(30))
    }

    for ($attempt = 0; $attempt -lt 30; $attempt++) {
        $online = Test-RunnerOnline
        if ($online) { return $online }
        Start-Sleep -Seconds 2
    }
    throw 'The runner service started, but GitHub did not report label sc220-build online within 60 seconds.'
}

function Stop-LocalRunner([System.ServiceProcess.ServiceController]$Service) {
    if (-not $Service) { return }
    $current = Get-Service -Name $Service.Name -ErrorAction SilentlyContinue
    if ($current -and $current.Status -ne 'Stopped') {
        Write-Host "Returning local runner offline: $($current.Name)"
        Stop-Service -Name $current.Name -Force -ErrorAction SilentlyContinue
    }
}

function Sync-CleanAskpSubmodule {
    $relativePath = 'third_party/askp-vst'
    $authority = Join-Path $root 'third_party\askp-vst'
    $gitMarker = Join-Path $authority '.git'

    if (Test-Path -LiteralPath $gitMarker) {
        $submoduleDirty = (& git.exe -C $authority status --porcelain 2>$null | Out-String).Trim()
        if ($LASTEXITCODE -ne 0) {
            Write-Warning 'ASK-P submodule state could not be inspected; leaving it untouched.'
            return
        }
        if ($submoduleDirty) {
            Write-Host 'ASK-P submodule contains local edits; automatic pointer repair is skipped.' -ForegroundColor Yellow
            return
        }
    }

    & git.exe submodule sync -- $relativePath *> $null
    if ($LASTEXITCODE -ne 0) {
        throw 'Unable to synchronize the ASK-P submodule URL.'
    }

    & git.exe submodule update --init --recursive --force --checkout -- $relativePath
    if ($LASTEXITCODE -ne 0) {
        throw 'Unable to synchronize the ASK-P submodule to the commit pinned by SC220 Live.'
    }
}

function Get-WorkflowRuns {
    $result = Get-GhJson run list `
        --repo $Repository `
        --workflow 'local-release.yml' `
        --branch $WorkflowRef `
        --event workflow_dispatch `
        --limit 10 `
        --json 'databaseId,createdAt,status,url,headBranch,headSha,displayTitle'

    if ($null -eq $result) { return @() }
    return @($result)
}

function Get-ActiveWorkflowRun {
    $activeStates = @('queued', 'in_progress', 'waiting', 'pending', 'requested')
    return @(Get-WorkflowRuns) |
        Where-Object { $activeStates -contains [string]$_.status } |
        Sort-Object { [Int64]$_.databaseId } -Descending |
        Select-Object -First 1
}

function Get-RunIdFromDispatchOutput([object[]]$Lines) {
    foreach ($line in @($Lines)) {
        $text = [string]$line
        if ($text -match '/actions/runs/([0-9]+)') {
            return [Int64]$Matches[1]
        }
    }
    return [Int64]0
}

function Wait-ForNewWorkflowRun([Int64]$PreviousRunId) {
    for ($attempt = 0; $attempt -lt 15; $attempt++) {
        if ($attempt -gt 0) { Start-Sleep -Seconds 1 }
        $run = @(Get-WorkflowRuns) |
            Where-Object { [Int64]$_.databaseId -gt $PreviousRunId } |
            Sort-Object { [Int64]$_.databaseId } -Descending |
            Select-Object -First 1
        if ($run) { return $run }
    }
    return $null
}

function Watch-ReleaseRun([Int64]$RunId, [string]$RunUrl) {
    Write-Host "Run URL: $RunUrl" -ForegroundColor Cyan
    Write-Host "Watching release run #$RunId. The runner will stay online until it finishes." -ForegroundColor DarkCyan
    & gh.exe run watch $RunId --repo $Repository --exit-status
    if ($LASTEXITCODE -ne 0) {
        throw "Local release workflow failed. Open $RunUrl for details."
    }
}

function Test-PublicReleaseExists([string]$WantedVersion) {
    & gh.exe api "repos/$DownloadRepository/releases/tags/v$WantedVersion" *> $null
    return $LASTEXITCODE -eq 0
}

$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
Set-Location $root

Write-Section 'SC220 Live - smart local build and automatic public release'

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

# Repair the common harmless state where the parent repo has moved its ASK-P
# gitlink but the local clean submodule worktree is still on the previous commit.
# Never overwrite real edits inside the private ASK-P checkout.
Sync-CleanAskpSubmodule

$dirty = (& git.exe status --porcelain | Out-String).Trim()
if ($dirty) {
    throw @"
Local files are not clean. Commit and push all intended changes first.
The release workflow builds a GitHub ref, not uncommitted files on this PC.

$dirty
"@
}

$currentBranchText = (& git.exe branch --show-current | Select-Object -First 1)
$currentBranch = if ($currentBranchText) { $currentBranchText.Trim() } else { '' }
if (-not $SourceRef) {
    $SourceRef = if ($currentBranch) { $currentBranch } else { 'main' }
}

Write-Host "Checking remote source ref: $SourceRef"
& git.exe fetch origin $SourceRef --quiet
if ($LASTEXITCODE -ne 0) {
    throw "Remote source ref $SourceRef was not found. Push the branch or tag before releasing."
}
$remoteSourceCommit = (& git.exe rev-parse FETCH_HEAD | Select-Object -First 1).Trim()

if ($currentBranch -eq $SourceRef) {
    $localHead = (& git.exe rev-parse HEAD | Select-Object -First 1).Trim()
    if ($localHead -ne $remoteSourceCommit) {
        $mergeBase = (& git.exe merge-base $localHead $remoteSourceCommit | Select-Object -First 1)
        if ($mergeBase -and $mergeBase.Trim() -eq $localHead) {
            Write-Host "Local $SourceRef is behind origin. Fast-forwarding automatically..." -ForegroundColor Yellow
            & git.exe merge --ff-only $remoteSourceCommit
            if ($LASTEXITCODE -ne 0) {
                throw "Automatic fast-forward of $SourceRef failed. Run git pull --ff-only manually."
            }
            Sync-CleanAskpSubmodule
            $localHead = (& git.exe rev-parse HEAD | Select-Object -First 1).Trim()
        }

        if ($localHead -ne $remoteSourceCommit) {
            throw "Local HEAD ($localHead) is not the pushed source ref ($remoteSourceCommit). Push or reconcile the branch before releasing."
        }
    }
}

$dirtyAfterSync = (& git.exe status --porcelain | Out-String).Trim()
if ($dirtyAfterSync) {
    throw "Release preflight changed or found local files unexpectedly:`n$dirtyAfterSync"
}

& gh.exe api "repos/$Repository/contents/.github/workflows/local-release.yml?ref=$WorkflowRef" *> $null
if ($LASTEXITCODE -ne 0) {
    throw @"
The local-release workflow is not available on $Repository@$WorkflowRef.
Merge the local-runner workflow into the default branch before the first release,
or pass -WorkflowRef with a branch already recognized by GitHub Actions.
"@
}

$runnerService = Get-LocalRunnerService
if (-not $runnerService) {
    throw 'SC220 runner service is missing. Run SETUP-LOCAL-RUNNER.cmd first.'
}
$runnerInfo = Get-CimInstance Win32_Service -Filter "Name='$($runnerService.Name)'"
if (-not $runnerInfo -or
    $runnerInfo.StartName -notin @('LocalSystem', 'NT AUTHORITY\SYSTEM') -or
    $runnerInfo.StartMode -ne 'Manual') {
    throw 'Runner service is not in release-only LocalSystem/Manual mode. Run SETUP-LOCAL-RUNNER.cmd again.'
}

$currentRelease = $null
try {
    $currentRelease = Invoke-RestMethod -UseBasicParsing `
        -Uri 'https://raw.githubusercontent.com/masarray/sc220-download/main/latest.json' `
        -Headers @{ 'Cache-Control' = 'no-cache'; 'User-Agent' = 'SC220-Local-Release/1.1' }
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

Write-Section 'Release confirmation'
Write-Host "Source       : $Repository@$SourceRef"
Write-Host "Source SHA   : $remoteSourceCommit"
Write-Host "Workflow ref : $WorkflowRef"
Write-Host "Version      : v$Version"
Write-Host "Channel      : $Channel"
Write-Host "Destination  : $DownloadRepository"
Write-Host "Build runner : local PC, release-only LocalSystem service"
Write-Host "Notes        : $ReleaseNotes"
Write-Host ''
$confirm = (Read-Host 'Type RELEASE to build, test, and publish').Trim()
if (-not $confirm.Equals('RELEASE', [System.StringComparison]::OrdinalIgnoreCase)) {
    throw 'Release cancelled by user.'
}

$releaseCompleted = $false
try {
    $runner = Start-And-WaitForRunner $runnerService
    Write-Host "Runner online : $($runner.name)" -ForegroundColor Green

    # A previous launcher may have crashed after workflow_dispatch succeeded.
    # Reattach to that unfinished run instead of creating a duplicate release.
    $activeRun = Get-ActiveWorkflowRun
    if ($activeRun) {
        $activeRunId = [Int64]$activeRun.databaseId
        $activeRunUrl = [string]$activeRun.url
        Write-Host "Found unfinished release run #$activeRunId ($($activeRun.status)). Reusing it instead of dispatching a duplicate." -ForegroundColor Yellow
        Watch-ReleaseRun -RunId $activeRunId -RunUrl $activeRunUrl

        if (Test-PublicReleaseExists $Version) {
            Write-Host "Existing run published v$Version successfully." -ForegroundColor Green
            $releaseCompleted = $true
        }
        else {
            Write-Host "The previous run completed but did not publish requested v$Version. Dispatching this request now." -ForegroundColor Yellow
        }
    }

    if (-not $releaseCompleted) {
        $runsBeforeDispatch = @(Get-WorkflowRuns)
        $previousRunId = if ($runsBeforeDispatch.Count -gt 0) {
            [Int64](($runsBeforeDispatch | Sort-Object { [Int64]$_.databaseId } -Descending | Select-Object -First 1).databaseId)
        } else {
            [Int64]0
        }

        $workflowArguments = @(
            'workflow', 'run', 'local-release.yml',
            '--repo', $Repository,
            '--ref', $WorkflowRef,
            '-f', "version=$Version",
            '-f', "source_ref=$SourceRef",
            '-f', 'clean=true',
            '-f', "release_channel=$Channel",
            '-f', "release_notes=$ReleaseNotes"
        )

        $dispatchOutput = @(Invoke-GhCapture @workflowArguments)
        $runId = Get-RunIdFromDispatchOutput -Lines $dispatchOutput
        $runUrl = if ($runId -gt 0) { "https://github.com/$Repository/actions/runs/$runId" } else { '' }

        if ($runId -le 0) {
            Write-Host 'Workflow accepted. Resolving run ID with a short numeric fallback...' -ForegroundColor DarkCyan
            $newRun = Wait-ForNewWorkflowRun -PreviousRunId $previousRunId
            if (-not $newRun) {
                throw 'Workflow was dispatched, but its run ID could not be located. The runner remains protected by the finally block.'
            }
            $runId = [Int64]$newRun.databaseId
            $runUrl = [string]$newRun.url
        }
        else {
            Write-Host "Workflow run ID received immediately: $runId" -ForegroundColor Green
        }

        Watch-ReleaseRun -RunId $runId -RunUrl $runUrl

        if (-not (Test-PublicReleaseExists $Version)) {
            throw "Workflow completed without a verifiable public release tag v$Version in $DownloadRepository."
        }
        $releaseCompleted = $true
    }
}
finally {
    Stop-LocalRunner $runnerService
}

if (-not $releaseCompleted) {
    throw "Release v$Version did not reach the verified public-release state."
}

$releaseUrl = "https://github.com/$DownloadRepository/releases/tag/v$Version"
Write-Section 'RELEASE BERHASIL'
Write-Host $releaseUrl -ForegroundColor Green
Start-Process $releaseUrl
