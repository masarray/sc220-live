param(
    [ValidatePattern('^$|^\d+\.\d+\.\d+$')]
    [string]$Version = '',
    [switch]$Clean
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version 2.0

$sourceRepository = 'masarray/sc220-live'
$publicRepository = 'masarray/sc220-download'
$sourceUrl = 'https://github.com/masarray/sc220-live.git'
$publicUrl = 'https://github.com/masarray/sc220-download.git'
$serviceBase = 'https://masarray-license-api.masari.workers.dev'
$releaseRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$publicCheckout = Join-Path $releaseRoot '.release-public'

function Invoke-Checked([string]$Label, [scriptblock]$Action) {
    Write-Host "==> $Label"
    & $Action
    if ($LASTEXITCODE -ne 0) { throw "$Label failed with exit code $LASTEXITCODE" }
}

function Remove-ReleaseCheckout {
    if (-not (Test-Path $publicCheckout)) { return }
    $resolvedRoot = [IO.Path]::GetFullPath($releaseRoot).TrimEnd('\') + '\'
    $resolvedTarget = [IO.Path]::GetFullPath($publicCheckout)
    if (-not $resolvedTarget.StartsWith($resolvedRoot, [StringComparison]::OrdinalIgnoreCase) -or
        [IO.Path]::GetFileName($resolvedTarget) -ne '.release-public') {
        throw "Refusing to remove unexpected release path: $resolvedTarget"
    }
    Remove-Item -LiteralPath $resolvedTarget -Recurse -Force
}

Set-Location $releaseRoot

foreach ($command in @('git.exe','gh.exe','curl.exe')) {
    if (-not (Get-Command $command -ErrorAction SilentlyContinue)) {
        throw "$command is required and was not found in PATH."
    }
}

if (-not $Version) {
    $projectLine = Select-String -Path (Join-Path $releaseRoot 'CMakeLists.txt') -Pattern '^project\(SC220Live VERSION ([0-9]+\.[0-9]+\.[0-9]+)' |
        Select-Object -First 1
    if (-not $projectLine) { throw 'Could not derive the product version from CMakeLists.txt.' }
    $Version = $projectLine.Matches[0].Groups[1].Value
}
$tag = "v$Version"

$currentBranch = (& git branch --show-current).Trim()
if ($currentBranch -ne 'main') {
    throw "Public release must run from main; current branch is '$currentBranch'. Merge the reviewed release candidate first."
}

& git diff --quiet
if ($LASTEXITCODE -ne 0) { throw 'Working tree has unstaged changes. Public release requires a clean tree.' }
& git diff --cached --quiet
if ($LASTEXITCODE -ne 0) { throw 'Working tree has staged changes. Public release requires a clean tree.' }

Invoke-Checked 'Fetching canonical private main branch' { git fetch origin main --quiet }
$sourceCommit = (& git rev-parse HEAD).Trim()
$remoteCommit = (& git rev-parse origin/main).Trim()
if ($sourceCommit -ne $remoteCommit) {
    throw "HEAD must exactly match origin/main. HEAD=$sourceCommit origin/main=$remoteCommit"
}

$receiptPath = Join-Path $releaseRoot 'dist\public-release-validation.json'
if (-not (Test-Path $receiptPath)) {
    throw 'Public release validation receipt is missing. Run scripts/approve-sc220-public-release.ps1 after completing docs/PUBLIC_RELEASE_GATE.md.'
}
$receipt = Get-Content -Raw -LiteralPath $receiptPath | ConvertFrom-Json
if ($receipt.product -ne 'SC220 Live' -or
    $receipt.version -ne $Version -or
    $receipt.commit -ne $sourceCommit) {
    throw 'Validation receipt does not match the exact product, version, and source commit being released.'
}
$validatedUtc = [DateTime]::Parse($receipt.validated_utc).ToUniversalTime()
if (([DateTime]::UtcNow - $validatedUtc).TotalHours -gt 72) {
    throw 'Validation receipt is older than 72 hours. Repeat the exact-commit release validation.'
}

if ($env:VBCABLE_PACK45_SHA256 -notmatch '^[A-Fa-f0-9]{64}$') {
    throw 'Public release requires repository-reviewed environment variable VBCABLE_PACK45_SHA256.'
}

$releaseNotesTemplate = Join-Path $releaseRoot "docs\RELEASE_NOTES_v$Version.md"
if (-not (Test-Path $releaseNotesTemplate)) {
    throw "Release notes are missing: $releaseNotesTemplate"
}
$releaseScreenshot = Join-Path $releaseRoot 'docs\SC220_LIVE_UX_LOCKED.png'
if (-not (Test-Path $releaseScreenshot)) {
    throw 'Final release screenshot docs/SC220_LIVE_UX_LOCKED.png is missing.'
}

$licenceText = Get-Content -Raw -LiteralPath (Join-Path $releaseRoot 'LICENSE')
$eulaText = Get-Content -Raw -LiteralPath (Join-Path $releaseRoot 'EULA.txt')
$purchaseText = Get-Content -Raw -LiteralPath (Join-Path $releaseRoot 'PURCHASE_TERMS.txt')
$thirdPartyText = Get-Content -Raw -LiteralPath (Join-Path $releaseRoot 'THIRD_PARTY_NOTICES.md')
if ($licenceText -notmatch 'PROPRIETARY SOFTWARE' -or
    $eulaText -notmatch '365-DAY' -or
    $eulaText -notmatch 'Audio processing' -or
    $eulaText -notmatch 'designed to continue' -or
    $purchaseText -notmatch 'IDR 399,000' -or
    $purchaseText -notmatch 'not a subscription' -or
    $thirdPartyText -notmatch 'VB-Audio VB-CABLE standard' -or
    $thirdPartyText -notmatch 'donationware') {
    throw 'Proprietary licence, 365-day safe continuation, purchase terms, or VB-CABLE notice are incomplete.'
}

& (Join-Path $PSScriptRoot 'check-sc220-release-contract.ps1')
& python (Join-Path $releaseRoot 'qa\audit_sc220_installer_bundle.py')
if ($LASTEXITCODE -ne 0) { throw 'Installer bundle contract audit failed.' }

$healthJson = (& curl.exe --silent --show-error --fail --max-time 20 "$serviceBase/health")
if ($LASTEXITCODE -ne 0 -or -not $healthJson) {
    throw 'Licence service health could not be verified.'
}
$health = $healthJson | ConvertFrom-Json
if (-not $health.ok -or $health.environment -ne 'production') {
    throw "Public release requires the production licence service; found environment '$($health.environment)'."
}

$productJson = (& curl.exe --silent --show-error --fail --max-time 20 "$serviceBase/v1/products/sc220-live")
if ($LASTEXITCODE -ne 0 -or -not $productJson) {
    throw 'Licence service product sc220-live is not available for purchase.'
}
$productEnvelope = $productJson | ConvertFrom-Json
$product = $productEnvelope.product
if (-not $productEnvelope.ok -or
    $product.id -ne 'sc220-live' -or
    [int]$product.price_idr -ne 399000 -or
    $product.currency -ne 'IDR' -or
    [int]$product.max_activations -ne 1 -or
    $product.license_mode -ne 'perpetual') {
    throw 'Licence service product contract does not match SC220 Live IDR 399,000 perpetual / one-computer terms.'
}

Invoke-Checked 'Checking GitHub authentication' { gh auth status }
$privateVisibility = (gh repo view $sourceRepository --json visibility --jq .visibility).Trim()
$publicVisibility = (gh repo view $publicRepository --json visibility --jq .visibility).Trim()
if ($privateVisibility -ne 'PRIVATE') { throw "$sourceRepository must be PRIVATE; found $privateVisibility." }
if ($publicVisibility -ne 'PUBLIC') { throw "$publicRepository must be PUBLIC; found $publicVisibility." }

$origin = (& git remote get-url origin 2>$null | Select-Object -First 1)
if (-not $origin -or $origin.TrimEnd('/') -notin @($sourceUrl.TrimEnd('/'), 'git@github.com:masarray/sc220-live.git')) {
    throw "origin points to an unexpected repository: $origin"
}

$trackedForbidden = & git ls-files | Where-Object {
    $_ -match '(^|/)(build|dist|\.release-|\.env)(/|$)' -or
    $_ -match '\.(exe|dll|pdb|obj|lib|exp|zip|pfx|p12|pem|key)$'
}
if (@($trackedForbidden).Count -gt 0) {
    throw "Private source repository contains generated or sensitive tracked files: $($trackedForbidden -join ', ')"
}

& gh release view $tag --repo $publicRepository *> $null
if ($LASTEXITCODE -eq 0) {
    throw "$publicRepository already has release $tag. Increase the version before publishing."
}

$portableArgs = @{ Version = $Version }
if ($Clean) { $portableArgs.Clean = $true }
$portablePackage = & (Join-Path $PSScriptRoot 'package-sc220-windows.ps1') @portableArgs
if (-not $portablePackage -or
    -not (Test-Path $portablePackage.Zip) -or
    -not (Test-Path $portablePackage.HashFile)) {
    throw 'Verified portable package was not produced.'
}

$installerPackage = & (Join-Path $PSScriptRoot 'package-sc220-installer.ps1') `
    -Version $Version `
    -VBCableSha256 $env:VBCABLE_PACK45_SHA256 `
    -SkipBuild
if (-not $installerPackage -or
    -not (Test-Path $installerPackage.Installer) -or
    -not (Test-Path $installerPackage.HashFile) -or
    -not (Test-Path $installerPackage.Manifest)) {
    throw 'Verified installer bundle was not produced.'
}

$headAfterBuild = (& git rev-parse HEAD).Trim()
if ($headAfterBuild -ne $sourceCommit) {
    throw 'Source commit changed during release build.'
}
& git diff --quiet
if ($LASTEXITCODE -ne 0) { throw 'Release build modified tracked source files.' }
& git diff --cached --quiet
if ($LASTEXITCODE -ne 0) { throw 'Release build staged tracked source files.' }

Remove-ReleaseCheckout
Invoke-Checked 'Cloning isolated public download repository' { git clone $publicUrl $publicCheckout }

Copy-Item -LiteralPath (Join-Path $releaseRoot 'release-site\index.html') -Destination $publicCheckout
Copy-Item -LiteralPath (Join-Path $releaseRoot 'release-site\styles.css') -Destination $publicCheckout
Copy-Item -LiteralPath (Join-Path $releaseRoot 'release-site\README.md') -Destination $publicCheckout
Copy-Item -LiteralPath (Join-Path $releaseRoot 'release-site\latest.json') -Destination $publicCheckout
Copy-Item -LiteralPath (Join-Path $releaseRoot 'assets\ui\recording_tech_logo.png') -Destination (Join-Path $publicCheckout 'recording-tech-logo.png')
Copy-Item -LiteralPath $releaseScreenshot -Destination (Join-Path $publicCheckout 'sc220-live-console.png')
Set-Content -LiteralPath (Join-Path $publicCheckout '.nojekyll') -Value '' -Encoding ASCII

$installerName = [IO.Path]::GetFileName($installerPackage.Installer)
$installerHashName = [IO.Path]::GetFileName($installerPackage.HashFile)
$installerManifestName = [IO.Path]::GetFileName($installerPackage.Manifest)
$portableName = [IO.Path]::GetFileName($portablePackage.Zip)
$portableHashName = [IO.Path]::GetFileName($portablePackage.HashFile)

$downloadUrl = "https://github.com/$publicRepository/releases/download/$tag/$installerName"
$hashUrl = "https://github.com/$publicRepository/releases/download/$tag/$installerHashName"
$manifestUrl = "https://github.com/$publicRepository/releases/download/$tag/$installerManifestName"
$portableUrl = "https://github.com/$publicRepository/releases/download/$tag/$portableName"
$portableHashUrl = "https://github.com/$publicRepository/releases/download/$tag/$portableHashName"
$date = Get-Date -Format 'yyyy-MM-dd'
$size = "{0:N1} MB" -f ((Get-Item -LiteralPath $installerPackage.Installer).Length / 1MB)
$portableSize = "{0:N1} MB" -f ((Get-Item -LiteralPath $portablePackage.Zip).Length / 1MB)

foreach ($templateFile in @('index.html','latest.json')) {
    $path = Join-Path $publicCheckout $templateFile
    $content = Get-Content -Raw -LiteralPath $path
    $content = $content.Replace('{{VERSION}}', $Version)
    $content = $content.Replace('{{DATE}}', $date)
    $content = $content.Replace('{{SIZE}}', $size)
    $content = $content.Replace('{{PORTABLE_SIZE}}', $portableSize)
    $content = $content.Replace('{{SHA256}}', $installerPackage.Sha256)
    $content = $content.Replace('{{VBCABLE_SHA256}}', $installerPackage.VBCableSha256)
    $content = $content.Replace('{{DOWNLOAD_URL}}', $downloadUrl)
    $content = $content.Replace('{{HASH_URL}}', $hashUrl)
    $content = $content.Replace('{{MANIFEST_URL}}', $manifestUrl)
    $content = $content.Replace('{{PORTABLE_URL}}', $portableUrl)
    $content = $content.Replace('{{PORTABLE_HASH_URL}}', $portableHashUrl)
    Set-Content -LiteralPath $path -Value $content -Encoding UTF8
}

$publicForbidden = Get-ChildItem -LiteralPath $publicCheckout -Recurse -File -Force |
    Where-Object { $_.FullName -notlike "*\.git\*" } |
    Where-Object {
        $_.Extension -in @('.c','.cc','.cpp','.cxx','.h','.hh','.hpp','.cmake','.exe','.dll','.pdb','.obj','.lib','.exp','.pfx','.p12','.pem','.key') -or
        $_.Name -in @('CMakeLists.txt','.gitmodules')
    }
if (@($publicForbidden).Count -gt 0) {
    throw "Public repository boundary violation: $($publicForbidden.FullName -join ', ')"
}

Push-Location $publicCheckout
try {
    Invoke-Checked 'Staging public landing page only' { git add --all }
    & git diff --cached --quiet
    if ($LASTEXITCODE -ne 0) {
        Invoke-Checked "Committing public landing page for $tag" { git commit -m "release: SC220 Live $tag downloads" }
        Invoke-Checked 'Pushing public landing page' { git push -u origin HEAD:main }
    }
} finally {
    Pop-Location
}

$notesPath = Join-Path $releaseRoot 'dist\release-notes.md'
$notes = Get-Content -Raw -LiteralPath $releaseNotesTemplate
$notes += @"

Installer SHA-256: ``$($installerPackage.Sha256)``
VB-CABLE Pack45 SHA-256: ``$($installerPackage.VBCableSha256)``
Portable SHA-256: ``$($portablePackage.Sha256)``
Private source commit: ``$sourceCommit``
"@
Set-Content -LiteralPath $notesPath -Value $notes -Encoding UTF8

Invoke-Checked 'Publishing binary-only GitHub release' {
    gh release create $tag `
        $installerPackage.Installer `
        $installerPackage.HashFile `
        $installerPackage.Manifest `
        $portablePackage.Zip `
        $portablePackage.HashFile `
        --repo $publicRepository `
        --title "SC220 Live $tag" `
        --notes-file $notesPath `
        --latest
}

& gh api "repos/$publicRepository/pages" *> $null
if ($LASTEXITCODE -ne 0) {
    Invoke-Checked 'Enabling GitHub Pages from main branch' {
        gh api --method POST "repos/$publicRepository/pages" -f build_type=legacy -f 'source[branch]=main' -f 'source[path]=/'
    }
}

$releaseJson = gh release view $tag --repo $publicRepository --json assets,url,isLatest
if ($LASTEXITCODE -ne 0) { throw 'Published release could not be read back for verification.' }
$releaseData = $releaseJson | ConvertFrom-Json
$assetNames = @($releaseData.assets | ForEach-Object { $_.name })
$expectedAssets = @(
    $installerName,
    $installerHashName,
    $installerManifestName,
    $portableName,
    $portableHashName
)
$missingAssets = @($expectedAssets | Where-Object { $_ -notin $assetNames })
if ($missingAssets.Count -gt 0 -or -not $releaseData.isLatest) {
    throw "Public release verification failed. Missing: $($missingAssets -join ', ')"
}

Write-Host "`nSC220 LIVE RELEASE VERIFIED" -ForegroundColor Green
Write-Host "Private source  : https://github.com/$sourceRepository/commit/$sourceCommit"
Write-Host "Public release  : $($releaseData.url)"
Write-Host "Landing page    : https://masarray.github.io/sc220-download/"
Write-Host "Installer hash  : $($installerPackage.Sha256)"
Write-Host "VB-CABLE hash   : $($installerPackage.VBCableSha256)"
Write-Host "Portable hash   : $($portablePackage.Sha256)"
