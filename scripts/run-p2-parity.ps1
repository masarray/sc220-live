param(
    [ValidateSet('Release','Debug')]
    [string]$Configuration = 'Release',
    [string]$BuildDir = 'build-live',
    [string]$JuceSourceDir = '',
    [string]$AskpSourceDir = '',
    [ValidateSet('Auto','VisualStudio','Ninja')]
    [string]$Generator = 'Auto',
    [switch]$Clean
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version 2.0

$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$buildScript = Join-Path $PSScriptRoot 'build-live-windows.ps1'
$build = Join-Path $root $BuildDir
$results = Join-Path $root 'qa-results\p2-parity'
New-Item -ItemType Directory -Force -Path $results | Out-Null

$buildArgs = @(
    '-ExecutionPolicy', 'Bypass',
    '-File', $buildScript,
    '-Configuration', $Configuration,
    '-BuildDir', $BuildDir,
    '-Generator', $Generator,
    '-ParityQa'
)
if ($Clean) { $buildArgs += '-Clean' }
if ($JuceSourceDir) { $buildArgs += @('-JuceSourceDir', $JuceSourceDir) }
if ($AskpSourceDir) { $buildArgs += @('-AskpSourceDir', $AskpSourceDir) }

Write-Host '==> Building P2 parity renderers'
& powershell @buildArgs
if ($LASTEXITCODE -ne 0) { throw "P2 parity build failed with exit code $LASTEXITCODE" }

function Find-Renderer([string]$pattern) {
    return Get-ChildItem $build -Recurse -File -Filter '*.exe' |
        Where-Object { $_.Name -match $pattern -or $_.BaseName -match $pattern } |
        Select-Object -First 1
}

$referenceExe = Find-Renderer 'SonkupikParityReference|Reference Renderer'
$liveExe = Find-Renderer 'SonkupikParityLive|Live Renderer'
if ($null -eq $referenceExe) { throw 'SonkupikParityReference executable was not found.' }
if ($null -eq $liveExe) { throw 'SonkupikParityLive executable was not found.' }

$referenceJson = Join-Path $results 'askp-vst-reference.json'
$liveJson = Join-Path $results 'sonkupik-live.json'
$report = Join-Path $results 'p2-preset-parity.txt'

Write-Host "==> Rendering askp-vst authority: $($referenceExe.FullName)"
& $referenceExe.FullName $referenceJson
if ($LASTEXITCODE -ne 0) { throw "Reference renderer failed with exit code $LASTEXITCODE" }

Write-Host "==> Rendering Sonkupik Live: $($liveExe.FullName)"
& $liveExe.FullName $liveJson
if ($LASTEXITCODE -ne 0) { throw "Live renderer failed with exit code $LASTEXITCODE" }

$reference = Get-Content -Raw $referenceJson | ConvertFrom-Json
$live = Get-Content -Raw $liveJson | ConvertFrom-Json

foreach ($key in @('sampleRate','blockSize','durationSeconds','analysisStartSeconds','presetCount')) {
    if ($reference.$key -ne $live.$key) {
        throw "Render configuration differs for ${key}: $($reference.$key) vs $($live.$key)"
    }
}

$lines = [System.Collections.Generic.List[string]]::new()
$lines.Add('Sonkupik Live P2 preset-by-preset audio parity')
$lines.Add("Reference: $referenceJson")
$lines.Add("Live: $liveJson")
$lines.Add('')
$failed = 0

for ($i = 0; $i -lt $reference.presets.Count; $i++) {
    $ref = $reference.presets[$i]
    $candidate = $live.presets[$i]
    $reasons = [System.Collections.Generic.List[string]]::new()

    foreach ($key in @('index','id','name','latencySamples','analysedSamples')) {
        if ($ref.$key -ne $candidate.$key) {
            $reasons.Add("${key}: $($ref.$key) != $($candidate.$key)")
        }
    }
    if ($ref.fnv1a64 -ne $candidate.fnv1a64) {
        $reasons.Add("hash: $($ref.fnv1a64) != $($candidate.fnv1a64)")
    }

    $rmsDelta = [math]::Abs([double]$ref.rms - [double]$candidate.rms)
    $peakDelta = [math]::Abs([double]$ref.peak - [double]$candidate.peak)
    if ($rmsDelta -gt 1.0e-12) { $reasons.Add("rms delta: $rmsDelta") }
    if ($peakDelta -gt 1.0e-12) { $reasons.Add("peak delta: $peakDelta") }

    $identity = ('{0:D2} {1} / {2}' -f [int]$ref.index, $ref.id, $ref.name)
    if ($reasons.Count -eq 0) {
        $lines.Add("PASS  $identity  $($ref.fnv1a64)")
    } else {
        $failed++
        $lines.Add("FAIL  $identity")
        foreach ($reason in $reasons) { $lines.Add("      $reason") }
    }
}

$passed = $reference.presets.Count - $failed
$lines.Add('')
$lines.Add("Presets checked: $($reference.presets.Count)")
$lines.Add("Passed: $passed")
$lines.Add("Failed: $failed")
$lines.Add('Result: ' + $(if ($failed -eq 0) { 'PASS' } else { 'FAIL' }))
$lines | Set-Content -Encoding utf8 $report
$lines | ForEach-Object { Write-Host $_ }

if ($failed -ne 0) {
    throw "P2 audio parity failed for $failed preset(s). See $report"
}

Write-Host "`nP2 PARITY PASS: $report" -ForegroundColor Green
