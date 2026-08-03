param(
    [Parameter(Mandatory = $true)]
    [ValidatePattern('^\d+\.\d+\.\d+$')]
    [string]$Version,
    [switch]$Clean
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version 2.0

$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$dist = Join-Path $root 'dist'
$name = "SC220-Live-v$Version-win-x64-portable"
$stage = Join-Path $dist $name
$zip = Join-Path $dist "$name.zip"

$buildArgs = @{ Configuration = 'Release'; NoExplorer = $true }
if ($Clean) { $buildArgs.Clean = $true }
& (Join-Path $PSScriptRoot 'build-sc220-one-click.ps1') @buildArgs

$exe = Join-Path $root 'build-sc220\SC220Live_artefacts\Release\SC220 Live.exe'
if (-not (Test-Path $exe)) {
    $exe = Get-ChildItem (Join-Path $root 'build-sc220') -Recurse -Filter 'SC220 Live.exe' |
        Select-Object -First 1 -ExpandProperty FullName
}
if (-not $exe) { throw 'Release executable was not found after a successful build.' }

if (Test-Path $stage) { Remove-Item -LiteralPath $stage -Recurse -Force }
if (Test-Path $zip) { Remove-Item -LiteralPath $zip -Force }
if (Test-Path "$zip.sha256") { Remove-Item -LiteralPath "$zip.sha256" -Force }
New-Item -ItemType Directory -Force -Path $stage | Out-Null

Copy-Item -LiteralPath $exe -Destination (Join-Path $stage 'SC220 Live.exe')
Copy-Item -LiteralPath (Join-Path $root 'THIRD_PARTY_NOTICES.md') -Destination $stage
Copy-Item -LiteralPath (Join-Path $root 'EULA.txt') -Destination $stage
Copy-Item -LiteralPath (Join-Path $root 'PURCHASE_TERMS.txt') -Destination $stage

$quickStart = @'
SC220 LIVE - QUICK START

1. Install VB-Audio VB-CABLE from its official publisher.
2. Connect Recording Tech SC220 and close apps that use it exclusively.
3. Open SC220 Live and press SETTINGS.
4. Select SC220 as input and CABLE Input (VB-Audio Virtual Cable) as output.
5. Keep GO LIVE off while setting levels. Avoid red meter segments.
6. Enable GO LIVE, then select CABLE Output in OBS or TikTok Live Studio.
7. PC AUDIO captures the default Windows playback mix. DUCK lowers it while SC220 is active.

This portable build does not silently install drivers and is not a VB-CABLE distributor.
'@
Set-Content -LiteralPath (Join-Path $stage 'QUICK_START.txt') -Value $quickStart -Encoding UTF8

$forbidden = Get-ChildItem -LiteralPath $stage -Recurse -File | Where-Object {
    $_.Extension -in @('.c','.cc','.cpp','.cxx','.h','.hh','.hpp','.cmake','.obj','.pdb','.lib','.exp') -or
    $_.Name -in @('CMakeLists.txt','.gitmodules')
}
if (@($forbidden).Count -gt 0) {
    throw "Source/debug material entered the public package: $($forbidden.FullName -join ', ')"
}

Compress-Archive -Path (Join-Path $stage '*') -DestinationPath $zip -CompressionLevel Optimal
$hash = (Get-FileHash -LiteralPath $zip -Algorithm SHA256).Hash.ToLowerInvariant()
Set-Content -LiteralPath "$zip.sha256" -Value "$hash  $([IO.Path]::GetFileName($zip))" -Encoding ASCII

Write-Host "PACKAGE: $zip" -ForegroundColor Green
Write-Host "SHA256 : $hash" -ForegroundColor Green
[pscustomobject]@{ Zip = $zip; HashFile = "$zip.sha256"; Sha256 = $hash; Name = $name }
