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
- Install VB-Audio VB-CABLE separately from its official publisher.
- For processed PC Audio monitoring, use one virtual-cable family as the PC Audio source and a different family as Stream Output.

RECOMMENDED ROUTING
1. In Windows Volume Mixer or the music application's output setting, route the music/browser/player to VB-CABLE family A.
2. Open SC220 Live.
3. PC AUDIO SOURCE: select the same VB-CABLE family A.
4. STREAM OUTPUT: select a different VB-CABLE family B used by OBS or the live application.
5. MONITOR OUTPUT: select physical headphones or speakers.
6. Select PROGRAM to hear the protected final mix, or MUSIC CUE to hear enhanced PC Music before ducking.
7. Keep Stream and Monitor muted while setting levels. Avoid red meter segments.
8. Enable Stream, then select the matching CABLE Output endpoint in OBS or the live application.

SC220
- Connect Recording Tech SC220 when Voice input is required.
- SC220 Live recognises endpoints named MIXER, SC220, or Recording Tech.
- PC Music can operate without SC220 connected.

IMPORTANT ANTI-ECHO RULE
Do not use the same physical endpoint as both PC AUDIO SOURCE and MONITOR OUTPUT. Windows would play the original sound directly and SC220 Live would return a delayed DSP copy, causing echo or feedback. The application blocks unsafe same-endpoint and same-cable-family routes where possible.

This portable build does not install audio drivers and does not redistribute VB-CABLE.
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
