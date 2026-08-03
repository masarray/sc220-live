param()

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version 2.0

$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path

function Read-Required([string]$RelativePath) {
    $path = Join-Path $root $RelativePath
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "Release contract file is missing: $RelativePath"
    }
    return Get-Content -Raw -LiteralPath $path
}

function Assert-Contains([string]$Text, [string]$Pattern, [string]$Description) {
    if ($Text -notmatch $Pattern) {
        throw "SC220 release contract failed: $Description"
    }
}

function Assert-Literal([string]$Text, [string]$Value, [string]$Description) {
    if (-not $Text.Contains($Value)) {
        throw "SC220 release contract failed: $Description"
    }
}

$cmake = Read-Required 'CMakeLists.txt'
$licenseHeader = Read-Required 'third_party\askp-vst\src\LicenseManager.h'
$licenseSource = Read-Required 'third_party\askp-vst\src\LicenseManager.cpp'
$bridge = Read-Required 'live\EnhancerEngineBridge.cpp'
$main = Read-Required 'live\SC220MainComponent.cpp'
$engine = Read-Required 'live\LiveAudioEngine.cpp'
$checkout = Read-Required 'live\LicenseCheckoutClient.cpp'
$package = Read-Required 'scripts\package-sc220-windows.ps1'
$release = Read-Required 'scripts\release-sc220-windows.ps1'
$licence = Read-Required 'LICENSE'
$eula = Read-Required 'EULA.txt'
$purchase = Read-Required 'PURCHASE_TERMS.txt'

Assert-Contains $licenseHeader 'betaDurationDays\s*=\s*365' 'ASK-P authority must provide exactly 365 free-control days.'
Assert-Contains $cmake 'SONKUPIK_LICENSE_PRODUCT_ID="sc220-live"' 'the checkout product ID must be sc220-live.'
Assert-Contains $cmake 'SONKUPIK_LICENSE_PRICE_IDR=399000' 'the compiled checkout price must be IDR 399,000.'
Assert-Contains $cmake 'SC220-LIVE-LOCAL-STATE-V1-' 'SC220 must have a product-specific protected state salt.'
Assert-Contains $cmake 'SC220-LIVE-HARDWARE-FINGERPRINT-V1' 'SC220 must have a product-specific hardware fingerprint.'
Assert-Literal $cmake 'HKEY_CURRENT_USER\\\\Software\\\\MasArray\\\\SC220 Live\\\\' 'SC220 must have a separate registry state path.'
Assert-Literal $cmake '.getChildFile(\"SC220 Live\")' 'SC220 must have a separate application-data path.'
Assert-Contains $licenseSource 'ASKP-R1-' 'the existing signed request transport must remain compatible.'
Assert-Contains $licenseSource 'ASKP-A1-' 'the existing signed activation transport must remain compatible.'

foreach ($setter in @('setPresetIndex', 'setEnhance', 'setSmartBass', 'setSmartTreble', 'setVocalBody', 'setStereoMagic')) {
    Assert-Contains $bridge ("void EnhancerEngineBridge::" + $setter + '[\s\S]{0,220}if \(! editingAllowed\(\)\) return;') "$setter must fail closed after the free-control period."
}
Assert-Contains $main 'presetBox\.setEnabled \(creativeEditing\)' 'the preset selector must visibly become read-only.'
Assert-Contains $main 'knob\.setEnabled \(creativeEditing\)' 'the five creative macros must visibly become read-only.'
if ($engine -match 'ClockNeedsAttention|State::ReadOnly|isLicensed\(\)') {
    throw 'SC220 release contract failed: licence state must not mute or stop the realtime mixer/audio engine.'
}

Assert-Contains $checkout 'kMaxApiResponseBytes' 'checkout responses must remain bounded.'
Assert-Contains $checkout 'app\.midtrans\.com' 'only trusted Midtrans checkout hosts may open.'
Assert-Contains $checkout 'checkout-state-v2\.json' 'pending checkout recovery must remain durable.'
Assert-Contains $checkout 'Do not pay again' 'duplicate-payment recovery guidance must remain present.'

foreach ($legalFile in @('EULA.txt', 'PURCHASE_TERMS.txt', 'THIRD_PARTY_NOTICES.md')) {
    Assert-Contains $package ([regex]::Escape($legalFile)) "$legalFile must be included in every binary package."
}
Assert-Contains $licence 'PROPRIETARY SOFTWARE' 'the private source authority must remain proprietary.'
Assert-Contains $eula '365-DAY FREE FULL-CONTROL' 'the EULA must state the one-year free-control term.'
Assert-Contains $eula 'designed to continue' 'the EULA must preserve safe post-period audio operation.'
Assert-Contains $purchase 'IDR 399,000' 'purchase terms must match the compiled price.'
Assert-Contains $purchase 'not a subscription' 'purchase terms must reject subscriptions and auto-renewal.'
Assert-Contains $release 'Public repository boundary violation' 'the public-repository source-code gate must remain enabled.'
Assert-Contains $release 'environment -ne ''production''' 'public release must reject a sandbox payment service.'
Assert-Contains $release 'v1/products/sc220-live' 'public release must verify the deployed SC220 product catalogue.'
Assert-Contains $release 'price_idr -ne 399000' 'public release must verify the deployed product price.'

Write-Host '[CONTRACT] SC220 proprietary, 365-day, checkout, safe-audio, and binary-only checks passed.' -ForegroundColor Green
