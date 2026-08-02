param(
    [string]$RepositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$commit = '5757119cab7a1a75f09dcf0edea6e3be97975439'
$relativePath = 'third_party/askp-vst'
$authority = Join-Path $RepositoryRoot 'third_party\askp-vst'
$authorityParent = Split-Path -Parent $authority
$gitMarker = Join-Path $authority '.git'

if (-not (Get-Command git.exe -ErrorAction SilentlyContinue)) {
    throw 'Git is required to initialise the pinned private ASK-P authority.'
}

function Invoke-Git {
    param(
        [Parameter(Mandatory = $true)]
        [string[]]$Arguments,
        [switch]$AllowFailure
    )

    $lines = @(& git.exe @Arguments 2>&1)
    $exitCode = $LASTEXITCODE
    if (-not $AllowFailure -and $exitCode -ne 0) {
        $details = ($lines | ForEach-Object { $_.ToString() }) -join [Environment]::NewLine
        throw "git $($Arguments -join ' ') failed with exit code $exitCode.`n$details"
    }

    return [pscustomobject]@{
        ExitCode = $exitCode
        Lines = $lines
    }
}

function Get-AuthorityCommit {
    # A real submodule worktree has its own .git file/directory. Without this
    # guard, `git -C third_party/askp-vst rev-parse HEAD` can accidentally read
    # the Sonkupik parent repository when a stale copied source folder exists.
    if (-not (Test-Path -LiteralPath $authority -PathType Container) -or
        -not (Test-Path -LiteralPath $gitMarker)) {
        return $null
    }

    $result = Invoke-Git -Arguments @(
        '-C', $authority,
        'rev-parse', '--verify', 'HEAD'
    ) -AllowFailure

    if ($result.ExitCode -ne 0) {
        return $null
    }

    $line = $result.Lines |
        ForEach-Object { $_.ToString().Trim() } |
        Where-Object { $_ -match '^[0-9a-fA-F]{40}$' } |
        Select-Object -First 1

    return $line
}

function Move-StaleAuthorityAside {
    if (-not (Test-Path -LiteralPath $authority)) {
        return
    }

    $stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
    $backup = Join-Path $authorityParent "askp-vst.stale-$stamp"
    $suffix = 1
    while (Test-Path -LiteralPath $backup) {
        $backup = Join-Path $authorityParent "askp-vst.stale-$stamp-$suffix"
        $suffix++
    }

    Write-Host '==> Existing ASK-P folder is not a valid Git worktree.' -ForegroundColor Yellow
    Write-Host "    Preserving it as: $backup" -ForegroundColor DarkYellow
    Move-Item -LiteralPath $authority -Destination $backup -Force
}

# Fail early when the local branch does not actually contain the reviewed
# submodule gitlink. This usually means the user has not fetched/checked out the
# complete port branch yet.
$treeResult = Invoke-Git -Arguments @(
    '-C', $RepositoryRoot,
    'ls-tree', 'HEAD', '--', $relativePath
) -AllowFailure
$treeLine = ($treeResult.Lines | ForEach-Object { $_.ToString() }) -join "`n"
if ($treeResult.ExitCode -ne 0 -or
    $treeLine -notmatch "^160000\s+commit\s+$commit\s+") {
    throw @"
The current Sonkupik branch does not contain the pinned ASK-P v0.5.23 gitlink.
Expected: $relativePath -> $commit

Refresh the port branch first:
  git fetch origin
  git checkout port/askp-v0.5.23
  git pull --ff-only
"@
}

$actual = Get-AuthorityCommit
if ($null -eq $actual) {
    Move-StaleAuthorityAside
}

if ($actual -ne $commit) {
    Write-Host '==> Initialising ASK-P v0.5.23 authority submodule' -ForegroundColor Yellow

    # Clear broken administrative state left by interrupted or older submodule
    # layouts. Failure here is harmless when the module was never initialised.
    Invoke-Git -Arguments @(
        '-C', $RepositoryRoot,
        'submodule', 'deinit', '-f', '--', $relativePath
    ) -AllowFailure | Out-Null

    $moduleAdmin = Join-Path $RepositoryRoot '.git\modules\third_party\askp-vst'
    if ((Test-Path -LiteralPath $moduleAdmin) -and
        -not (Test-Path -LiteralPath $authority)) {
        Remove-Item -LiteralPath $moduleAdmin -Recurse -Force
    }

    $sync = Invoke-Git -Arguments @(
        '-C', $RepositoryRoot,
        'submodule', 'sync', '--', $relativePath
    ) -AllowFailure
    if ($sync.ExitCode -ne 0) {
        $details = ($sync.Lines | ForEach-Object { $_.ToString() }) -join [Environment]::NewLine
        throw "git submodule sync failed.`n$details"
    }

    $update = Invoke-Git -Arguments @(
        '-C', $RepositoryRoot,
        'submodule', 'update', '--init', '--recursive',
        '--force', '--checkout', '--', $relativePath
    ) -AllowFailure
    if ($update.ExitCode -ne 0) {
        $details = ($update.Lines | ForEach-Object { $_.ToString() }) -join [Environment]::NewLine
        throw @"
Unable to initialise the private masarray/askp-vst authority.

Git output:
$details

Authenticate Git Credential Manager with an account that can read both private repositories, then run:
  git submodule sync -- third_party/askp-vst
  git submodule update --init --recursive --force --checkout -- third_party/askp-vst
"@
    }

    $actual = Get-AuthorityCommit
}

if ($null -eq $actual) {
    throw @"
ASK-P checkout completed without a readable submodule worktree.
Run these diagnostics and share the complete output:
  git submodule status -- third_party/askp-vst
  git -C third_party/askp-vst status
  type third_party\askp-vst\.git
"@
}

if ($actual -ne $commit) {
    throw "ASK-P authority mismatch. Expected $commit but found $actual"
}

Write-Host "==> ASK-P authority verified: v0.5.23 @ $commit" -ForegroundColor Green
