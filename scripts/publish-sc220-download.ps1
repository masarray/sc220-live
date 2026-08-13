param(
    [Parameter(Mandatory = $true)]
    [ValidatePattern('^\d+\.\d+\.\d+$')]
    [string]$Version,

    [Parameter(Mandatory = $true)]
    [ValidatePattern('^[A-Fa-f0-9]{40}$')]
    [string]$SourceCommit,

    [string]$DistDir = '',
    [string]$TargetRepository = 'masarray/sc220-download',

    [ValidateSet('stable', 'prerelease')]
    [string]$Channel = 'stable',

    [string]$ReleaseNotes = '',
    [switch]$ReplaceExisting
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version 2.0

$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
if (-not $DistDir) { $DistDir = Join-Path $root 'dist' }
$DistDir = (Resolve-Path -LiteralPath $DistDir).Path

function Read-AuthorityLock([string]$Path) {
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "Authority lock is missing: $Path"
    }
    $values = @{}
    foreach ($line in Get-Content -LiteralPath $Path) {
        if ($line -match '^([^=]+)=(.*)$') {
            $values[$Matches[1].Trim()] = $Matches[2].Trim()
        }
    }
    return $values
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
    $text = (& gh.exe @Arguments 2>$null | Out-String)
    if ($LASTEXITCODE -ne 0 -or [string]::IsNullOrWhiteSpace($text)) { return $null }
    return $text | ConvertFrom-Json
}

if (-not (Get-Command gh.exe -ErrorAction SilentlyContinue)) {
    throw 'GitHub CLI is required for the public draft handoff.'
}
& gh.exe auth status *> $null
if ($LASTEXITCODE -ne 0) {
    throw 'GitHub CLI must be authenticated with write access to masarray/sc220-download.'
}

$tag = "v$Version"
$installerName = "SC220-Live-v$Version-Setup-win-x64.exe"
$checksumName = "$installerName.sha256"
$manifestName = "SC220-Live-v$Version-Setup-win-x64.manifest.json"
$installer = Join-Path $DistDir $installerName
$checksum = Join-Path $DistDir $checksumName
$manifestPath = Join-Path $DistDir $manifestName

foreach ($required in @($installer, $checksum, $manifestPath)) {
    if (-not (Test-Path -LiteralPath $required -PathType Leaf)) {
        throw "Release asset is missing: $required"
    }
}

$installerHash = (Get-FileHash -LiteralPath $installer -Algorithm SHA256).Hash.ToLowerInvariant()
$checksumText = (Get-Content -LiteralPath $checksum -Raw).Trim()
if ($checksumText -notmatch '^([A-Fa-f0-9]{64})\s+') {
    throw "$checksumName does not contain a valid SHA-256 line."
}
if ($Matches[1].ToLowerInvariant() -ne $installerHash) {
    throw "Installer checksum file does not match $installerName."
}

$manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
if ([string]$manifest.version -ne $Version) {
    throw "Manifest version $($manifest.version) does not match requested $Version."
}
if ([string]$manifest.installer_sha256 -ne $installerHash) {
    throw 'Manifest installer_sha256 does not match the built installer.'
}

$vbLock = Read-AuthorityLock (Join-Path $root 'VB_CABLE_AUTHORITY.lock')
$vcLock = Read-AuthorityLock (Join-Path $root 'VC_REDIST_AUTHORITY.lock')
$vbHash = [string]$vbLock['package_sha256']
$vcHash = [string]$vcLock['package_sha256']
foreach ($value in @($vbHash, $vcHash)) {
    if ($value -notmatch '^[A-Fa-f0-9]{64}$') {
        throw 'One or more prerequisite authority locks contain an invalid SHA-256.'
    }
}

$buildCommitFile = Join-Path $DistDir 'BUILD_COMMIT.txt'
$vbHashFile = Join-Path $DistDir 'VB_CABLE_PACK45_SHA256.txt'
$vcHashFile = Join-Path $DistDir 'VC_REDIST_X64_SHA256.txt'
Set-Content -LiteralPath $buildCommitFile -Value $SourceCommit -Encoding ASCII
Set-Content -LiteralPath $vbHashFile -Value $vbHash.ToLowerInvariant() -Encoding ASCII
Set-Content -LiteralPath $vcHashFile -Value $vcHash.ToLowerInvariant() -Encoding ASCII

$highlight = if ([string]::IsNullOrWhiteSpace($ReleaseNotes)) {
    'Smart Windows installer with automatic runtime, Windows Audio, and VB-CABLE readiness checks.'
} else {
    $ReleaseNotes.Trim()
}

$releaseBody = @"
# SC220 Live $tag

$highlight

## Smart installation

- Detects and installs the verified Microsoft Visual C++ x64 Runtime when required.
- Detects Windows Audio and Audio Endpoint Builder service readiness.
- Keeps a healthy existing VB-Audio VB-CABLE installation unchanged.
- Offers install or repair when standard VB-CABLE is absent or incomplete.
- Prevents first launch when a prerequisite failed or Windows must restart.

## Installation

1. Download `$installerName`.
2. Run the installer as Administrator.
3. Review the automatic system check.
4. Restart Windows when requested after a runtime or driver operation.
5. In OBS or TikTok Live Studio, use `CABLE Output` as the audio input when VB-CABLE routing is enabled.

## Integrity

- Installer SHA-256: `$installerHash`
- Private source commit: `$SourceCommit`
- Microsoft VC++ Runtime package SHA-256: `$($vcHash.ToLowerInvariant())`
- VB-CABLE Pack45 SHA-256: `$($vbHash.ToLowerInvariant())`
- Platform: Windows 10/11 x64

The installer was built and audited directly on the approved local Windows workstation. The public sc220-download workflow independently inspects the binary-only draft before publication.

Windows SmartScreen may still show an unknown-publisher warning until a dedicated application code-signing certificate is deployed.
"@

$notesPath = Join-Path $DistDir "RELEASE_NOTES_v$Version.md"
Set-Content -LiteralPath $notesPath -Value $releaseBody -Encoding UTF8

$assets = @(
    $installer,
    $checksum,
    $manifestPath,
    $buildCommitFile,
    $vcHashFile,
    $vbHashFile
)
$requiredAssetNames = @($assets | ForEach-Object { [IO.Path]::GetFileName($_) })

$existingRelease = Get-GhJson release view $tag --repo $TargetRepository --json 'url,assets,isDraft,isPrerelease,tagName'
if ($existingRelease) {
    $existingNames = @($existingRelease.assets | ForEach-Object { $_.name })
    $missing = @($requiredAssetNames | Where-Object { $existingNames -notcontains $_ })
    $installerAsset = @($existingRelease.assets) | Where-Object { $_.name -eq $installerName } | Select-Object -First 1
    $digestMatches = $installerAsset -and
        $installerAsset.digest -eq "sha256:$installerHash"

    if ($missing.Count -eq 0 -and $digestMatches -and $existingRelease.isDraft -and -not $ReplaceExisting) {
        Write-Host "Existing verified draft $tag will be reused." -ForegroundColor Yellow
    }
    elseif ($ReplaceExisting) {
        Write-Host "Deleting existing release $tag before replacement..." -ForegroundColor Yellow
        Invoke-Gh release delete $tag --repo $TargetRepository --cleanup-tag --yes
        $existingRelease = $null
    }
    else {
        throw "Release $tag already exists, is published, incomplete, or has a different installer digest. Use an unused version or explicit replacement."
    }
}

if (-not $existingRelease) {
    $arguments = @(
        'release', 'create', $tag,
        '--repo', $TargetRepository,
        '--target', 'main',
        '--title', "SC220 Live $tag",
        '--notes-file', $notesPath,
        '--draft'
    )
    if ($Channel -eq 'prerelease') { $arguments += '--prerelease' }
    $arguments += $assets
    Invoke-Gh @arguments
}

$draft = Get-GhJson release view $tag --repo $TargetRepository --json 'url,assets,isDraft,isPrerelease,tagName,publishedAt'
if (-not $draft -or -not $draft.isDraft -or $draft.publishedAt) {
    throw "Binary handoff $tag is not an unpublished draft."
}
$draftNames = @($draft.assets | ForEach-Object { $_.name })
$missingAfterUpload = @($requiredAssetNames | Where-Object { $draftNames -notcontains $_ })
if ($missingAfterUpload.Count -gt 0) {
    throw "Draft release is missing assets: $($missingAfterUpload -join ', ')"
}

$dispatchStarted = [DateTime]::UtcNow.AddMinutes(-1)
Invoke-Gh workflow run 'publish-draft-release.yml' `
    --repo $TargetRepository `
    -f "version=$Version"

$publicRun = $null
for ($attempt = 0; $attempt -lt 20 -and -not $publicRun; $attempt++) {
    Start-Sleep -Seconds 1
    $runs = Get-GhJson run list `
        --repo $TargetRepository `
        --workflow 'publish-draft-release.yml' `
        --event workflow_dispatch `
        --limit 10 `
        --json 'databaseId,status,conclusion,url,createdAt'
    $publicRun = @($runs | Where-Object {
        [DateTime]$_.createdAt -ge $dispatchStarted
    } | Sort-Object { [long]$_.databaseId } -Descending) | Select-Object -First 1
}
if (-not $publicRun) {
    throw 'Public publish-draft-release workflow was dispatched but no run ID appeared.'
}

Invoke-Gh run watch ([string]$publicRun.databaseId) `
    --repo $TargetRepository `
    --exit-status

$published = Get-GhJson release view $tag --repo $TargetRepository --json 'url,assets,isDraft,tagName,publishedAt'
$latest = Get-GhJson api "repos/$TargetRepository/releases/latest"
if (-not $published -or $published.isDraft -or -not $published.publishedAt -or
    -not $latest -or [string]$latest.tag_name -ne $tag) {
    throw "Public workflow completed without publishing and promoting $tag."
}

Write-Host "`nSC220 PUBLIC DRAFT HANDOFF OK" -ForegroundColor Green
Write-Host "Public CI : $($publicRun.url)"
Write-Host "Release   : $($published.url)"
Write-Host "SHA-256  : $installerHash"
