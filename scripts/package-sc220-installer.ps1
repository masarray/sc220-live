param(
    [Parameter(Mandatory = $true)]
    [ValidatePattern('^\d+\.\d+\.\d+$')]
    [string]$Version,

    [string]$VBCableZipPath = '',
    [string]$VBCableSha256 = $env:VBCABLE_PACK45_SHA256,
    [string]$VCRedistPath = '',
    [string]$VCRedistSha256 = $env:VC_REDIST_X64_SHA256,
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
$downloadedVBCableZip = Join-Path $work 'VBCABLE_Driver_Pack45.zip'
$expandedVBCable = Join-Path $work 'vbcable-pack45'
$downloadedVCRedist = Join-Path $work 'VC_redist.x64.exe'
$generatedIss = Join-Path $work 'SC220Live.generated.iss'
$installerTemplate = Join-Path $root 'installer\SC220Live.iss'
$prerequisiteProbe = Join-Path $root 'installer\SC220PrerequisiteProbe.ps1'
$vbNotice = Join-Path $root 'installer\VB_AUDIO_NOTICE.txt'
$vcAuthorityLock = Join-Path $root 'VC_REDIST_AUTHORITY.lock'

function Read-AuthorityLock([string]$Path) {
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "Authority lock is missing: $Path"
    }

    $result = @{}
    foreach ($line in Get-Content -LiteralPath $Path) {
        if ($line -match '^([^=]+)=(.*)$') {
            $result[$Matches[1].Trim()] = $Matches[2].Trim()
        }
    }
    return $result
}

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

function Copy-Or-Download(
    [string]$SuppliedPath,
    [string]$SourceUrl,
    [string]$Destination,
    [string]$Description
) {
    if ($SuppliedPath) {
        if (-not (Test-Path -LiteralPath $SuppliedPath -PathType Leaf)) {
            throw "Supplied $Description package does not exist: $SuppliedPath"
        }
        Copy-Item -LiteralPath $SuppliedPath -Destination $Destination -Force
        return
    }

    Write-Host "==> Downloading $Description"
    [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
    Invoke-WebRequest -UseBasicParsing -MaximumRedirection 5 `
        -Uri $SourceUrl `
        -OutFile $Destination `
        -UserAgent 'SC220-Live-Release-Builder/0.2'
}

if ([Environment]::OSVersion.Platform -ne [PlatformID]::Win32NT) {
    throw 'SC220 installer packaging must run on Windows.'
}
if (-not [Environment]::Is64BitOperatingSystem) {
    throw 'SC220 Live requires a 64-bit Windows build and packaging host.'
}
foreach ($required in @($installerTemplate, $prerequisiteProbe, $vbNotice, $vcAuthorityLock)) {
    if (-not (Test-Path -LiteralPath $required -PathType Leaf)) {
        throw "Installer source is missing: $required"
    }
}

$vcLock = Read-AuthorityLock $vcAuthorityLock
$officialVCRedistUrl = [string]$vcLock['source']
$lockedVCRedistHash = [string]$vcLock['package_sha256']
$vcSignerExpectation = [string]$vcLock['setup_signer_contains']

if ($officialVCRedistUrl -notmatch '^https://download\.visualstudio\.microsoft\.com/') {
    throw 'VC_REDIST_AUTHORITY.lock must pin an immutable download.visualstudio.microsoft.com URL.'
}
if ($lockedVCRedistHash -notmatch '^[A-Fa-f0-9]{64}$') {
    throw 'VC_REDIST_AUTHORITY.lock does not contain a valid package_sha256.'
}
if ([string]::IsNullOrWhiteSpace($vcSignerExpectation)) {
    throw 'VC_REDIST_AUTHORITY.lock does not define setup_signer_contains.'
}
if ([string]::IsNullOrWhiteSpace($VCRedistSha256)) {
    $VCRedistSha256 = $lockedVCRedistHash
}
if ($VCRedistSha256 -notmatch '^[A-Fa-f0-9]{64}$') {
    throw 'VC_REDIST_X64_SHA256 must contain the reviewed SHA-256 of VC_redist.x64.exe.'
}
if ($VCRedistSha256.ToLowerInvariant() -ne $lockedVCRedistHash.ToLowerInvariant()) {
    throw 'The requested VC Redistributable hash does not match VC_REDIST_AUTHORITY.lock.'
}
$expectedVCRedistHash = $VCRedistSha256.ToLowerInvariant()

if ([string]::IsNullOrWhiteSpace($VBCableSha256) -or
    $VBCableSha256 -notmatch '^[A-Fa-f0-9]{64}$') {
    throw @'
VBCABLE_PACK45_SHA256 must contain the reviewed SHA-256 of the current official
VBCABLE_Driver_Pack45.zip. The workflow intentionally refuses an unpinned driver.
Set VBCABLE_PACK45_SHA256 or pass -VBCableSha256 explicitly.
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
if (Test-Path -LiteralPath $expandedVBCable) {
    Remove-Item -LiteralPath $expandedVBCable -Recurse -Force
}
New-Item -ItemType Directory -Force -Path $expandedVBCable | Out-Null

Copy-Or-Download `
    -SuppliedPath $VBCableZipPath `
    -SourceUrl $officialVBCableUrl `
    -Destination $downloadedVBCableZip `
    -Description 'official VB-Audio VBCABLE_Driver_Pack45.zip'

$actualVBCableHash = (Get-FileHash -LiteralPath $downloadedVBCableZip -Algorithm SHA256).Hash.ToLowerInvariant()
if ($actualVBCableHash -ne $expectedVBCableHash) {
    throw @"
VB-CABLE supply-chain verification failed.
Expected: $expectedVBCableHash
Actual:   $actualVBCableHash
Source:   $officialVBCableUrl
Review the official package before updating VBCABLE_PACK45_SHA256.
"@
}

Expand-Archive -LiteralPath $downloadedVBCableZip -DestinationPath $expandedVBCable -Force

$forbiddenAdditionalCable = Get-ChildItem -LiteralPath $expandedVBCable -Recurse -File | Where-Object {
    $_.Name -match '(?i)VBCABLE[_-]?[ABCD](?:_|-|\.)' -or
    $_.FullName -match '(?i)VBCABLE_[ABCD]_Driver'
}
if (@($forbiddenAdditionalCable).Count -gt 0) {
    throw "The standard bundle unexpectedly contains A/B/C/D cable material: $($forbiddenAdditionalCable.FullName -join ', ')"
}

$vbSetup = Get-ChildItem -LiteralPath $expandedVBCable -Recurse -File -Filter 'VBCABLE_Setup_x64.exe' |
    Select-Object -First 1
if (-not $vbSetup) {
    throw 'Official package does not contain VBCABLE_Setup_x64.exe.'
}

$vbPackageRoot = $vbSetup.Directory.FullName
$vbPackageFiles = @(Get-ChildItem -LiteralPath $vbPackageRoot -Recurse -File)
if ($vbPackageFiles.Count -lt 5) {
    throw 'Official VB-CABLE package is incomplete beside VBCABLE_Setup_x64.exe.'
}
foreach ($extension in @('.inf', '.sys', '.cat')) {
    if (-not ($vbPackageFiles | Where-Object { $_.Extension -ieq $extension } | Select-Object -First 1)) {
        throw "Official VB-CABLE package is missing required driver support files: $extension"
    }
}

$vbSetupSignature = Get-AuthenticodeSignature -LiteralPath $vbSetup.FullName
if ($vbSetupSignature.Status -ne [System.Management.Automation.SignatureStatus]::Valid -or
    -not $vbSetupSignature.SignerCertificate) {
    throw "VBCABLE_Setup_x64.exe does not have a valid Authenticode signature: $($vbSetupSignature.Status)"
}

$catalogs = @(Get-ChildItem -LiteralPath $vbPackageRoot -Recurse -File -Filter '*.cat')
if ($catalogs.Count -eq 0) {
    throw 'Official VB-CABLE package contains no Windows driver catalogue.'
}
foreach ($catalog in $catalogs) {
    $catalogSignature = Get-AuthenticodeSignature -LiteralPath $catalog.FullName
    if ($catalogSignature.Status -ne [System.Management.Automation.SignatureStatus]::Valid) {
        throw "VB-CABLE catalogue signature is invalid: $($catalog.FullName) [$($catalogSignature.Status)]"
    }
}

Copy-Or-Download `
    -SuppliedPath $VCRedistPath `
    -SourceUrl $officialVCRedistUrl `
    -Destination $downloadedVCRedist `
    -Description 'pinned Microsoft Visual C++ Redistributable x64'

$actualVCRedistHash = (Get-FileHash -LiteralPath $downloadedVCRedist -Algorithm SHA256).Hash.ToLowerInvariant()
if ($actualVCRedistHash -ne $expectedVCRedistHash) {
    throw @"
Microsoft VC++ Redistributable supply-chain verification failed.
Expected: $expectedVCRedistHash
Actual:   $actualVCRedistHash
Source:   $officialVCRedistUrl
Review Microsoft package changes before updating VC_REDIST_AUTHORITY.lock.
"@
}

$vcSignature = Get-AuthenticodeSignature -LiteralPath $downloadedVCRedist
if ($vcSignature.Status -ne [System.Management.Automation.SignatureStatus]::Valid -or
    -not $vcSignature.SignerCertificate) {
    throw "VC_redist.x64.exe does not have a valid Authenticode signature: $($vcSignature.Status)"
}
if ($vcSignature.SignerCertificate.Subject -notlike "*$vcSignerExpectation*") {
    throw "Unexpected VC Redistributable signer: $($vcSignature.SignerCertificate.Subject)"
}
$vcFileVersion = (Get-Item -LiteralPath $downloadedVCRedist).VersionInfo.FileVersion

$iscc = Resolve-InnoCompiler $InnoCompiler
$defines = [System.Text.StringBuilder]::new()
Write-IsppDefine $defines 'AppVersion' $Version
Write-IsppDefine $defines 'AppExeSource' (Resolve-Path -LiteralPath $appExe).Path
Write-IsppDefine $defines 'EulaSource' (Resolve-Path -LiteralPath (Join-Path $root 'EULA.txt')).Path
Write-IsppDefine $defines 'PurchaseTermsSource' (Resolve-Path -LiteralPath (Join-Path $root 'PURCHASE_TERMS.txt')).Path
Write-IsppDefine $defines 'ThirdPartySource' (Resolve-Path -LiteralPath (Join-Path $root 'THIRD_PARTY_NOTICES.md')).Path
Write-IsppDefine $defines 'VBNoticeSource' (Resolve-Path -LiteralPath $vbNotice).Path
Write-IsppDefine $defines 'VBPackageSource' (Resolve-Path -LiteralPath $vbPackageRoot).Path
Write-IsppDefine $defines 'VCRedistSource' (Resolve-Path -LiteralPath $downloadedVCRedist).Path
Write-IsppDefine $defines 'PrerequisiteProbeSource' (Resolve-Path -LiteralPath $prerequisiteProbe).Path
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
    smart_prerequisites = [ordered]@{
        probe = 'Tools/SC220PrerequisiteProbe.ps1'
        vc_runtime_registry = 'HKLM/SOFTWARE/Microsoft/VisualStudio/14.0/VC/Runtimes/x64'
        audio_services = @('AudioEndpointBuilder', 'Audiosrv')
        vb_cable_endpoints = @('CABLE Input', 'CABLE Output')
    }
    vc_runtime = [ordered]@{
        product = 'Microsoft Visual C++ Redistributable v14 x64'
        package = 'VC_redist.x64.exe'
        source = $officialVCRedistUrl
        sha256 = $actualVCRedistHash
        file_version = $vcFileVersion
        signer = $vcSignature.SignerCertificate.Subject
        authenticode = $vcSignature.Status.ToString()
        install_arguments = '/install /passive /norestart'
    }
    vb_cable = [ordered]@{
        product = 'VB-Audio VB-CABLE standard'
        package = 'VBCABLE_Driver_Pack45.zip'
        source = $officialVBCableUrl
        sha256 = $actualVBCableHash
        setup = 'VBCABLE_Setup_x64.exe'
        embedded_package_root = [IO.Path]::GetFileName($vbPackageRoot)
        embedded_file_count = $vbPackageFiles.Count
        required_support_files = @('.inf', '.sys', '.cat')
        signer = $vbSetupSignature.SignerCertificate.Subject
        authenticode = $vbSetupSignature.Status.ToString()
        donationware_notice = 'installer/VB_AUDIO_NOTICE.txt'
    }
    built_utc = [DateTime]::UtcNow.ToString('o')
}
$manifest | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $manifestPath -Encoding UTF8

Write-Host "`nSC220 LIVE SMART INSTALLER BUNDLE OK" -ForegroundColor Green
Write-Host "Installer       : $installer"
Write-Host "SHA-256        : $installerHash"
Write-Host "VC++ Runtime   : $actualVCRedistHash [$vcFileVersion]"
Write-Host "VC++ Signer    : $($vcSignature.SignerCertificate.Subject)"
Write-Host "VB-CABLE       : $actualVBCableHash"
Write-Host "VB files       : $($vbPackageFiles.Count)"
Write-Host "VB signer      : $($vbSetupSignature.SignerCertificate.Subject)"

[pscustomobject]@{
    Installer = $installer
    HashFile = $hashPath
    Manifest = $manifestPath
    Sha256 = $installerHash
    VCRedistSha256 = $actualVCRedistHash
    VBCableSha256 = $actualVBCableHash
}
