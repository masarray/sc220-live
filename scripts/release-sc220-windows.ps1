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

$licenceText = Get-Content -Raw -LiteralPath (Join-Path $releaseRoot 'LICENSE')
$eulaText = Get-Content -Raw -LiteralPath (Join-Path $releaseRoot 'EULA.txt')
$purchaseText = Get-Content -Raw -LiteralPath (Join-Path $releaseRoot 'PURCHASE_TERMS.txt')
if ($licenceText -notmatch 'PROPRIETARY SOFTWARE' -or
    $eulaText -notmatch '365-DAY' -or
    $eulaText -notmatch 'Audio processing' -or
    $eulaText -notmatch 'designed to continue' -or
    $purchaseText -notmatch 'IDR 399,000' -or
    $purchaseText -notmatch 'not a subscription') {
    throw 'Proprietary licence, 365-day safe continuation, or purchase terms are incomplete.'
}

& (Join-Path $PSScriptRoot 'check-sc220-release-contract.ps1')

$serviceBase = 'https://masarray-license-api.masari.workers.dev'
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

if (-not $Version) {
    $projectLine = Select-String -Path (Join-Path $releaseRoot 'CMakeLists.txt') -Pattern '^project\(SC220Live VERSION ([0-9]+\.[0-9]+\.[0-9]+)' |
        Select-Object -First 1
    if (-not $projectLine) { throw 'Could not derive the product version from CMakeLists.txt.' }
    $Version = $projectLine.Matches[0].Groups[1].Value
}
$tag = "v$Version"

$origin = (& git remote get-url origin 2>$null | Select-Object -First 1)
if (-not $origin) {
    Invoke-Checked 'Configuring private source remote' { git remote add origin $sourceUrl }
} elseif ($origin.TrimEnd('/') -notin @($sourceUrl.TrimEnd('/'), 'git@github.com:masarray/sc220-live.git')) {
    throw "origin points to an unexpected repository: $origin"
}

$trackedForbidden = & git ls-files | Where-Object {
    $_ -match '(^|/)(build|dist|\.release-|\.env)(/|$)' -or
    $_ -match '\.(exe|dll|pdb|obj|lib|exp|zip|pfx|p12|pem|key)$'
}
if (@($trackedForbidden).Count -gt 0) {
    throw "Private source repository contains generated or sensitive tracked files: $($trackedForbidden -join ', ')"
}

$packageArgs = @{ Version = $Version }
if ($Clean) { $packageArgs.Clean = $true }
$package = & (Join-Path $PSScriptRoot 'package-sc220-windows.ps1') @packageArgs
if (-not $package -or -not (Test-Path $package.Zip) -or -not (Test-Path $package.HashFile)) {
    throw 'Verified release package was not produced.'
}

Invoke-Checked 'Staging private source' { git add --all }
$stagedForbidden = & git diff --cached --name-only | Where-Object {
    $_ -match '(^|/)(build|dist|\.release-|\.env)(/|$)' -or
    $_ -match '\.(exe|dll|pdb|obj|lib|exp|zip|pfx|p12|pem|key)$'
}
if (@($stagedForbidden).Count -gt 0) {
    throw "Release staging found forbidden private artifacts: $($stagedForbidden -join ', ')"
}

& git diff --cached --quiet
if ($LASTEXITCODE -ne 0) {
    Invoke-Checked "Committing private source for $tag" { git commit -m "release: SC220 Live $tag" }
}
$sourceCommit = (& git rev-parse HEAD).Trim()
Invoke-Checked 'Pushing private source authority' { git push -u origin HEAD:main }
$remoteCommit = (& git ls-remote $sourceUrl refs/heads/main | ForEach-Object { ($_ -split '\s+')[0] }).Trim()
if ($remoteCommit -ne $sourceCommit) { throw 'Private source push could not be verified against origin/main.' }

& gh release view $tag --repo $publicRepository *> $null
if ($LASTEXITCODE -eq 0) { throw "$publicRepository already has release $tag. Increase the version before publishing." }

Remove-ReleaseCheckout
Invoke-Checked 'Cloning isolated public download repository' { git clone $publicUrl $publicCheckout }

Copy-Item -LiteralPath (Join-Path $releaseRoot 'release-site\index.html') -Destination $publicCheckout
Copy-Item -LiteralPath (Join-Path $releaseRoot 'release-site\styles.css') -Destination $publicCheckout
Copy-Item -LiteralPath (Join-Path $releaseRoot 'release-site\README.md') -Destination $publicCheckout
Copy-Item -LiteralPath (Join-Path $releaseRoot 'release-site\latest.json') -Destination $publicCheckout
Copy-Item -LiteralPath (Join-Path $releaseRoot 'assets\ui\recording_tech_logo.png') -Destination (Join-Path $publicCheckout 'recording-tech-logo.png')
Set-Content -LiteralPath (Join-Path $publicCheckout '.nojekyll') -Value '' -Encoding ASCII

$zipName = [IO.Path]::GetFileName($package.Zip)
$hashName = [IO.Path]::GetFileName($package.HashFile)
$downloadUrl = "https://github.com/$publicRepository/releases/download/$tag/$zipName"
$hashUrl = "https://github.com/$publicRepository/releases/download/$tag/$hashName"
$date = Get-Date -Format 'yyyy-MM-dd'
$size = "{0:N1} MB" -f ((Get-Item -LiteralPath $package.Zip).Length / 1MB)

foreach ($templateFile in @('index.html','latest.json')) {
    $path = Join-Path $publicCheckout $templateFile
    $content = Get-Content -Raw -LiteralPath $path
    $content = $content.Replace('{{VERSION}}', $Version)
    $content = $content.Replace('{{DATE}}', $date)
    $content = $content.Replace('{{SIZE}}', $size)
    $content = $content.Replace('{{SHA256}}', $package.Sha256)
    $content = $content.Replace('{{DOWNLOAD_URL}}', $downloadUrl)
    $content = $content.Replace('{{HASH_URL}}', $hashUrl)
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
    Invoke-Checked "Committing public landing page for $tag" { git commit -m "release: SC220 Live $tag downloads" }
    Invoke-Checked 'Pushing public landing page' { git push -u origin HEAD:main }
} finally {
    Pop-Location
}

$notesPath = Join-Path $releaseRoot 'dist\release-notes.md'
$notes = @"
## SC220 Live $tag

- Native Windows x64 audio console for Recording Tech SC220
- SC220 plus default Windows PC Audio loopback
- SC220-keyed PC Audio ducking
- ASK-P Signature DSP authority v0.5.23
- Stream output intended for VB-CABLE

SHA-256: ``$($package.Sha256)``
Private source commit: ``$sourceCommit``
"@
Set-Content -LiteralPath $notesPath -Value $notes -Encoding UTF8

Invoke-Checked 'Publishing binary-only GitHub release' {
    gh release create $tag $package.Zip $package.HashFile --repo $publicRepository --title "SC220 Live $tag" --notes-file $notesPath --latest
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
if ($zipName -notin $assetNames -or $hashName -notin $assetNames -or -not $releaseData.isLatest) {
    throw 'Public release verification failed: expected assets or latest marker are missing.'
}

Write-Host "`nSC220 LIVE RELEASE VERIFIED" -ForegroundColor Green
Write-Host "Private source : https://github.com/$sourceRepository/commit/$sourceCommit"
Write-Host "Public release : $($releaseData.url)"
Write-Host "Landing page   : https://masarray.github.io/sc220-download/"
Write-Host "SHA-256        : $($package.Sha256)"
