param(
    [string]$RepositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
)

$ErrorActionPreference = 'Stop'
$commit = '5757119cab7a1a75f09dcf0edea6e3be97975439'
$relativePath = 'third_party/askp-vst'
$authority = Join-Path $RepositoryRoot 'third_party\askp-vst'
$engine = Join-Path $authority 'engine\arsonkupik_dsp.cpp'

if (-not (Get-Command git.exe -ErrorAction SilentlyContinue)) {
    throw 'Git is required to initialise the pinned private ASK-P authority.'
}

if (-not (Test-Path $engine)) {
    Write-Host '==> Initialising ASK-P v0.5.23 authority submodule' -ForegroundColor Yellow
    & git.exe -C $RepositoryRoot submodule sync -- $relativePath
    if ($LASTEXITCODE -ne 0) { throw 'git submodule sync failed.' }
    & git.exe -C $RepositoryRoot submodule update --init --recursive --force --checkout -- $relativePath
    if ($LASTEXITCODE -ne 0) {
        throw @"
Unable to initialise masarray/askp-vst.
Authenticate Git Credential Manager with an account that can read both private repositories, then run:
  git submodule update --init --recursive --force --checkout third_party/askp-vst
"@
    }
}

$actual = (& git.exe -C $authority rev-parse HEAD 2>$null | Select-Object -First 1).Trim()
if ($LASTEXITCODE -ne 0 -or -not $actual) {
    throw 'Unable to read the ASK-P authority commit.'
}
if ($actual -ne $commit) {
    & git.exe -C $RepositoryRoot submodule update --init --recursive --force --checkout -- $relativePath
    if ($LASTEXITCODE -ne 0) { throw 'Unable to restore the pinned ASK-P authority commit.' }
    $actual = (& git.exe -C $authority rev-parse HEAD | Select-Object -First 1).Trim()
}
if ($actual -ne $commit) {
    throw "ASK-P authority mismatch. Expected $commit but found $actual"
}

Write-Host "==> ASK-P authority verified: v0.5.23 @ $commit" -ForegroundColor Green
