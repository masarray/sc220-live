param(
    [ValidatePattern('^$|^\d+\.\d+\.\d+$')]
    [string]$Version = ''
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version 2.0

$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
Set-Location $root

foreach ($command in @('git.exe')) {
    if (-not (Get-Command $command -ErrorAction SilentlyContinue)) {
        throw "$command is required and was not found in PATH."
    }
}

if (-not $Version) {
    $projectLine = Select-String -Path (Join-Path $root 'CMakeLists.txt') -Pattern '^project\(SC220Live VERSION ([0-9]+\.[0-9]+\.[0-9]+)' |
        Select-Object -First 1
    if (-not $projectLine) { throw 'Could not derive the product version from CMakeLists.txt.' }
    $Version = $projectLine.Matches[0].Groups[1].Value
}

$currentBranch = (& git branch --show-current).Trim()
if ($currentBranch -ne 'main') {
    throw "Public release validation must run on main; current branch is '$currentBranch'."
}

& git diff --quiet
if ($LASTEXITCODE -ne 0) { throw 'Working tree has unstaged changes.' }
& git diff --cached --quiet
if ($LASTEXITCODE -ne 0) { throw 'Working tree has staged changes.' }

& git fetch origin main --quiet
if ($LASTEXITCODE -ne 0) { throw 'Could not fetch origin/main.' }
$head = (& git rev-parse HEAD).Trim()
$remote = (& git rev-parse origin/main).Trim()
if ($head -ne $remote) {
    throw "HEAD must exactly match origin/main before public validation. HEAD=$head origin/main=$remote"
}

$short = (& git rev-parse --short=8 HEAD).Trim()
$gate = Join-Path $root 'docs\PUBLIC_RELEASE_GATE.md'
if (-not (Test-Path $gate)) { throw 'docs/PUBLIC_RELEASE_GATE.md is missing.' }

Write-Host ''
Write-Host 'SC220 LIVE PUBLIC RELEASE VALIDATION' -ForegroundColor Cyan
Write-Host "Version : $Version"
Write-Host "Commit  : $head"
Write-Host ''
Write-Host 'Before approving, complete every mandatory Windows listening and presentation check in:'
Write-Host "  $gate"
Write-Host ''
Write-Host 'This includes the exact packaged build, distinct source/stream cables, physical monitoring,'
Write-Host 'endpoint switching, silence decay, sample-rate conversion, reconnect, ten-minute playback,'
Write-Host 'and final release screenshot validation.'
Write-Host ''

$phrase = "RELEASE $Version $short"
$typed = Read-Host "Type exactly '$phrase' to create the validation receipt"
if ($typed -cne $phrase) {
    throw 'Validation phrase did not match. No release receipt was created.'
}

$dist = Join-Path $root 'dist'
New-Item -ItemType Directory -Force -Path $dist | Out-Null
$receiptPath = Join-Path $dist 'public-release-validation.json'
$receipt = [ordered]@{
    product = 'SC220 Live'
    version = $Version
    commit = $head
    validated_utc = [DateTime]::UtcNow.ToString('o')
    attestation = 'All mandatory checks in docs/PUBLIC_RELEASE_GATE.md completed for this exact commit.'
}
$receipt | ConvertTo-Json | Set-Content -LiteralPath $receiptPath -Encoding UTF8

Write-Host ''
Write-Host "PUBLIC RELEASE RECEIPT CREATED: $receiptPath" -ForegroundColor Green
Write-Host 'The release script will reject a different commit, version, or stale receipt.'
