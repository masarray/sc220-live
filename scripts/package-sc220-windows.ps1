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

REQUIREMENTS
- Windows 10/11 x64.
- Install the standard VB-Audio VB-CABLE separately when using this portable package.
- The regular public setup installer can install standard VB-CABLE as an optional donationware component.

QUICK STREAM ROUTING
1. Keep music, browser, or player audio on the normal Windows speaker or headphone.
2. Open SC220 Live.
3. PC AUDIO SOURCE: select that same normal Windows playback endpoint.
4. STREAM OUTPUT: select CABLE Input (VB-Audio Virtual Cable).
5. In OBS or the live application, select CABLE Output as the audio source.
6. Keep the SC220 Monitor Bus off when it would return processed audio to the same physical endpoint being captured.
7. A genuinely different physical endpoint may be used for PROGRAM or MUSIC CUE monitoring.
8. Keep Stream and Monitor muted while setting levels. Avoid red meter segments.

SC220
- Connect Recording Tech SC220 when Voice input is required.
- SC220 Live recognises endpoints named MIXER, SC220, or Recording Tech.
- PC Music can operate without SC220 connected.

IMPORTANT ANTI-ECHO RULE
Do not use the same physical endpoint as both PC AUDIO SOURCE and MONITOR OUTPUT. Windows would play the original sound directly and SC220 Live would return a delayed DSP copy, causing echo or feedback. The application blocks unsafe same-endpoint and same-cable-family routes where possible.

VB-CABLE
- VB-CABLE is developed and owned by VB-Audio Software.
- It is distributed under the VB-Audio donationware model.
- Information and download: https://vb-audio.com/Cable/
- Restart Windows after installing or uninstalling the driver.

This portable build does not install audio drivers.
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
