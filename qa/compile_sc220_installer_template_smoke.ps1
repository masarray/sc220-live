param(
    [string]$InnoCompiler = ''
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version 2.0

$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$work = Join-Path $env:RUNNER_TEMP 'sc220-installer-template-smoke'
$payload = Join-Path $work 'payload'
$output = Join-Path $work 'output'
$generated = Join-Path $work 'SC220Live.smoke.iss'
$template = Join-Path $root 'installer\SC220Live.iss'

if (Test-Path -LiteralPath $work) {
    Remove-Item -LiteralPath $work -Recurse -Force
}
New-Item -ItemType Directory -Force -Path $payload, $output | Out-Null

$app = Join-Path $payload 'SC220 Live.exe'
$vbSetup = Join-Path $payload 'VBCABLE_Setup_x64.exe'
$eula = Join-Path $payload 'EULA.txt'
$purchase = Join-Path $payload 'PURCHASE_TERMS.txt'
$thirdParty = Join-Path $payload 'THIRD_PARTY_NOTICES.md'
$vbNotice = Join-Path $payload 'VB_AUDIO_NOTICE.txt'

Set-Content -LiteralPath $app -Value 'SC220 installer template smoke payload' -Encoding ASCII
Set-Content -LiteralPath $vbSetup -Value 'VB-CABLE installer placeholder - never executed' -Encoding ASCII
Set-Content -LiteralPath $eula -Value 'SC220 Live template smoke EULA' -Encoding ASCII
Set-Content -LiteralPath $purchase -Value 'SC220 Live template smoke purchase terms' -Encoding ASCII
Set-Content -LiteralPath $thirdParty -Value 'SC220 Live template smoke third-party notices' -Encoding ASCII
Set-Content -LiteralPath $vbNotice -Value 'VB-Audio template smoke notice' -Encoding ASCII

$candidates = [System.Collections.Generic.List[string]]::new()
if ($InnoCompiler) { $candidates.Add($InnoCompiler) }
$command = Get-Command ISCC.exe -ErrorAction SilentlyContinue
if ($command -and $command.Source) { $candidates.Add($command.Source) }
foreach ($base in @(${env:ProgramFiles(x86)}, $env:ProgramFiles)) {
    if ($base) { $candidates.Add((Join-Path $base 'Inno Setup 6\ISCC.exe')) }
}
$iscc = $candidates |
    Where-Object { $_ -and (Test-Path -LiteralPath $_ -PathType Leaf) } |
    Select-Object -First 1
if (-not $iscc) { throw 'Inno Setup compiler ISCC.exe was not found.' }

function Escape-Ispp([string]$Value) {
    if ($Value.Contains('"')) { throw "Unsupported quote in installer smoke path: $Value" }
    return $Value
}

$lines = @(
    "#define AppVersion `"0.1.0`"",
    "#define AppExeSource `"$(Escape-Ispp $app)`"",
    "#define EulaSource `"$(Escape-Ispp $eula)`"",
    "#define PurchaseTermsSource `"$(Escape-Ispp $purchase)`"",
    "#define ThirdPartySource `"$(Escape-Ispp $thirdParty)`"",
    "#define VBNoticeSource `"$(Escape-Ispp $vbNotice)`"",
    "#define VBSetupSource `"$(Escape-Ispp $vbSetup)`"",
    "#define OutputDir `"$(Escape-Ispp $output)`"",
    "#include `"$(Escape-Ispp $template)`""
)
Set-Content -LiteralPath $generated -Value $lines -Encoding UTF8

& $iscc $generated
if ($LASTEXITCODE -ne 0) {
    throw "Inno Setup template compilation failed with exit code $LASTEXITCODE"
}

$installer = Join-Path $output 'SC220-Live-v0.1.0-Setup-win-x64.exe'
if (-not (Test-Path -LiteralPath $installer -PathType Leaf)) {
    throw "Inno Setup did not produce the expected template installer: $installer"
}

Write-Host '[PASS] SC220 Live Inno Setup template compiled successfully.' -ForegroundColor Green
Write-Host "Template installer: $installer"
