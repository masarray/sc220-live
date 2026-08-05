param(
    [Parameter(Mandatory = $true)]
    [ValidatePattern('^\d+\.\d+\.\d+$')]
    [string]$Version,

    [string]$VBCableZipPath = '',
    [string]$VBCableSha256 = $env:VBCABLE_PACK45_SHA256,
    [string]$InnoCompiler = '',

    [switch]$Clean,
    [switch]$SkipBuild
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version 2.0

$officialVBCableUrl = 'https://download.vb-audio.com/Download_CABLE/VBCABLE_Driver_Pack45.zip'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$dist = Join-Path $root 'dist'
$work = Join-Path $root ".release-work\installer-v$Version"
$downloadedZip = Join-Path $work 'VBCABLE_Driver_Pack45.zip'
$expanded = Join-Path $work 'vbcable-pack45'
$generatedIss = Join-Path $work 'SC220Live.generated.iss'
$installerTemplate = Join-Path $root 'installer\SC220Live.iss'
$vbNotice = Join-Path $root 'installer\VB_AUDIO_NOTICE.txt'

function Resolve-InnoCompiler([string]$RequestedPath) {
    $candidates = [System.Collections.Generic.List[string]]::new()
    if ($RequestedPath) { $candidates.Add($RequestedPath) }

    $command = Get-Command ISCC.exe -ErrorAction SilentlyContinue
    if ($command -and $command.Source) { $candidates.Add($command.Source) }

    foreach ($base in @(${env:ProgramFiles(x86)}, $env:ProgramFiles)) {
        if ($base) {
            $candidates.Add((Join-Path $base 'Inno Setup 6\ISCC.exe'))
        }
    }

    $compiler = $candidates |
        Where-Object { $_ -and (Test-Path -LiteralPath $_ -PathType Leaf) } |
        Select-Object -First 1

    if (-not $compiler) {
        throw @'
Inno Setup 6 compiler was not found.
Install Inno Setup or run:
  choco install innosetup --version=6.7.1 -y
Then run this packager again.
'@
    }
    return (Resolve-Path -LiteralPath $compiler).Path
}

function Write-IsppDefine([System.Text.StringBuilder]$Builder, [string]$Name, [string]$Value) {
    if ($Value.Contains('"')) {
        throw "Inno Setup define '$Name' contains an unsupported quote character."
    }
    [void]$Builder.AppendLine("#define $Name `"$Value`"")
}

if ([Environment]::OSVersion.Platform -ne [PlatformID]::Win32NT) {
    throw 'SC220 installer packaging must run on Windows.'
}
if (-not [Environment]::Is64BitOperatingSystem) {
    throw 'SC220 Live requires a 64-bit Windows build and packaging host.'
}
if (-not (Test-Path -LiteralPath $installerTemplate -PathType Leaf)) {
    throw "Installer template is missing: $installerTemplate"
}
if (-not (Test-Path -LiteralPath $vbNotice -PathType Leaf)) {
    throw "VB-Audio notice is missing: $vbNotice"
}

if ([string]::IsNullOrWhiteSpace($VBCableSha256) -or
    $VBCableSha256 -notmatch '^[A-Fa-f0-9]{64}$') {
    throw @'
VBCABLE_PACK45_SHA256 must contain the reviewed SHA-256 of the current official
VBCABLE_Driver_Pack45.zip. The workflow intentionally refuses an unpinned driver.
Set repository variable VBCABLE_PACK45_SHA256 or pass -VBCableSha256 explicitly.
'@
}
$expectedVBCableHash = $VBCableSha256.ToLowerInvariant()

Set-Location $root
if (-not $SkipBuild) {
    $buildArgs = @{ Configuration = 'Release'; NoExplorer = $true }
    if ($Clean) { $buildArgs.Clean = $true }
    & (Join-Path $PSScriptRoot 'build-sc220-one-click.ps1') @buildArgs
}

$appExe = Join-Path $root 'build-sc220\SC220Live_artefacts\Release\SC220 Live.exe'
if (-not (Test-Path -LiteralPath $appExe -PathType Leaf)) {
    $appExe = Get-ChildItem (Join-Path $root 'build-sc220') -Recurse -Filter 'SC220 Live.exe' -File |
        Select-Object -First 1 -ExpandProperty FullName
}
if (-not $appExe -or -not (Test-Path -LiteralPath $appExe -PathType Leaf)) {
    throw 'Release executable was not found. Build SC220 Live before packaging the installer.'
}

foreach ($path in @($work, $dist)) {
    New-Item -ItemType Directory -Force -Path $path | Out-Null
}
if (Test-Path -LiteralPath $expanded) {
    Remove-Item -LiteralPath $expanded -Recurse -Force
}
New-Item -ItemType Directory -Force -Path $expanded | Out-Null

if ($VBCableZipPath) {
    if (-not (Test-Path -LiteralPath $VBCableZipPath -PathType Leaf)) {
        throw "Supplied VB-CABLE package does not exist: $VBCableZipPath"
    }
    Copy-Item -LiteralPath $VBCableZipPath -Destination $downloadedZip -Force
} else {
    Write-Host '==> Downloading official VB-Audio VBCABLE_Driver_Pack45.zip'
    [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
    Invoke-WebRequest -UseBasicParsing -MaximumRedirection 5 `
        -Uri $officialVBCableUrl `
        -OutFile $downloadedZip `
        -UserAgent 'SC220-Live-Release-Builder/0.1'
}

$actualVBCableHash = (Get-FileHash -LiteralPath $downloadedZip -Algorithm SHA256).Hash.ToLowerInvariant()
if ($actualVBCableHash -ne $expectedVBCableHash) {
    throw @"
VB-CABLE supply-chain verification failed.
Expected: $expectedVBCableHash
Actual:   $actualVBCableHash
Source:   $officialVBCableUrl
Review the official package before updating VBCABLE_PACK45_SHA256.
"@
}

Expand-Archive -LiteralPath $downloadedZip -DestinationPath $expanded -Force

$forbiddenAdditionalCable = Get-ChildItem -LiteralPath $expanded -Recurse -File | Where-Object {
    $_.Name -match '(?i)VBCABLE[_-]?[ABCD](?:_|-|\.)' -or
    $_.FullName -match '(?i)VBCABLE_[ABCD]_Driver'
}
if (@($forbiddenAdditionalCable).Count -gt 0) {
    throw "The standard bundle unexpectedly contains A/B/C/D cable material: $($forbiddenAdditionalCable.FullName -join ', ')"
}

$vbSetup = Get-ChildItem -LiteralPath $expanded -Recurse -File -Filter 'VBCABLE_Setup_x64.exe' |
    Select-Object -First 1
if (-not $vbSetup) {
    throw 'Official package does not contain VBCABLE_Setup_x64.exe.'
}

$setupSignature = Get-AuthenticodeSignature -LiteralPath $vbSetup.FullName
if ($setupSignature.Status -ne [System.Management.Automation.SignatureStatus]::Valid -or
    -not $setupSignature.SignerCertificate) {
    throw "VBCABLE_Setup_x64.exe does not have a valid Authenticode signature: $($setupSignature.Status)"
}

$catalogs = @(Get-ChildItem -LiteralPath $expanded -Recurse -File -Filter '*.cat')
if ($catalogs.Count -eq 0) {
    throw 'Official VB-CABLE package contains no Windows driver catalogue.'
}
foreach ($catalog in $catalogs) {
    $catalogSignature = Get-AuthenticodeSignature -LiteralPath $catalog.FullName
    if ($catalogSignature.Status -ne [System.Management.Automation.SignatureStatus]::Valid) {
        throw "VB-CABLE catalogue signature is invalid: $($catalog.FullName) [$($catalogSignature.Status)]"
    }
}

$iscc = Resolve-InnoCompiler $InnoCompiler
$defines = [System.Text.StringBuilder]::new()
Write-IsppDefine $defines 'AppVersion' $Version
Write-IsppDefine $defines 'AppExeSource' (Resolve-Path -LiteralPath $appExe).Path
Write-IsppDefine $defines 'EulaSource' (Resolve-Path -LiteralPath (Join-Path $root 'EULA.txt')).Path
Write-IsppDefine $defines 'PurchaseTermsSource' (Resolve-Path -LiteralPath (Join-Path $root 'PURCHASE_TERMS.txt')).Path
Write-IsppDefine $defines 'ThirdPartySource' (Resolve-Path -LiteralPath (Join-Path $root 'THIRD_PARTY_NOTICES.md')).Path
Write-IsppDefine $defines 'VBNoticeSource' (Resolve-Path -LiteralPath $vbNotice).Path
Write-IsppDefine $defines 'VBSetupSource' $vbSetup.FullName
Write-IsppDefine $defines 'OutputDir' (Resolve-Path -LiteralPath $dist).Path
[void]$defines.AppendLine("#include `"$installerTemplate`"")
Set-Content -LiteralPath $generatedIss -Value $defines.ToString() -Encoding UTF8

Write-Host "==> Compiling SC220 Live installer with $iscc"
& $iscc $generatedIss
if ($LASTEXITCODE -ne 0) {
    throw "Inno Setup failed with exit code $LASTEXITCODE"
}

$installer = Join-Path $dist "SC220-Live-v$Version-Setup-win-x64.exe"
if (-not (Test-Path -LiteralPath $installer -PathType Leaf)) {
    throw "Expected installer was not produced: $installer"
}

$installerHash = (Get-FileHash -LiteralPath $installer -Algorithm SHA256).Hash.ToLowerInvariant()
$hashPath = "$installer.sha256"
Set-Content -LiteralPath $hashPath `
    -Value "$installerHash  $([IO.Path]::GetFileName($installer))" `
    -Encoding ASCII

$sourceCommit = 'unknown'
if (Get-Command git.exe -ErrorAction SilentlyContinue) {
    $candidate = (& git rev-parse HEAD 2>$null | Select-Object -First 1)
    if ($candidate) { $sourceCommit = $candidate.Trim() }
}

$manifestPath = Join-Path $dist "SC220-Live-v$Version-Setup-win-x64.manifest.json"
$manifest = [ordered]@{
    product = 'SC220 Live'
    version = $Version
    platform = 'Windows 10/11 x64'
    source_commit = $sourceCommit
    installer_file = [IO.Path]::GetFileName($installer)
    installer_sha256 = $installerHash
    vb_cable = [ordered]@{
        product = 'VB-Audio VB-CABLE standard'
        package = 'VBCABLE_Driver_Pack45.zip'
        source = $officialVBCableUrl
        sha256 = $actualVBCableHash
        setup = 'VBCABLE_Setup_x64.exe'
        signer = $setupSignature.SignerCertificate.Subject
        authenticode = $setupSignature.Status.ToString()
        donationware_notice = 'installer/VB_AUDIO_NOTICE.txt'
    }
    built_utc = [DateTime]::UtcNow.ToString('o')
}
$manifest | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $manifestPath -Encoding UTF8

Write-Host "`nSC220 LIVE INSTALLER BUNDLE OK" -ForegroundColor Green
Write-Host "Installer : $installer"
Write-Host "SHA-256  : $installerHash"
Write-Host "VB-CABLE : $actualVBCableHash"
Write-Host "Signer   : $($setupSignature.SignerCertificate.Subject)"

[pscustomobject]@{
    Installer = $installer
    HashFile = $hashPath
    Manifest = $manifestPath
    Sha256 = $installerHash
    VBCableSha256 = $actualVBCableHash
}
