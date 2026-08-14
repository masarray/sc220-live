param(
    [ValidateSet('Release', 'Debug')]
    [string]$Configuration = 'Release',
    [string]$BuildDir = 'build-sc220',
    [switch]$Clean,
    [switch]$SkipBuild
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version 3.0

$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
Set-Location $root

$pythonCommand = Get-Command python.exe -ErrorAction SilentlyContinue
if (-not $pythonCommand) { $pythonCommand = Get-Command python -ErrorAction SilentlyContinue }
if (-not $pythonCommand) { throw 'Python is required for local SC220 source audits.' }
$python = $pythonCommand.Source

function Test-PythonModule([string]$Executable, [string]$Module) {
    $savedPreference = $ErrorActionPreference
    try {
        $ErrorActionPreference = 'SilentlyContinue'
        & $Executable -c "import $Module" *> $null
        return $LASTEXITCODE -eq 0
    }
    finally {
        $ErrorActionPreference = $savedPreference
    }
}

function Find-NumpyPython([string]$Preferred) {
    if (Test-PythonModule $Preferred 'numpy') { return $Preferred }

    $userProfile = [Environment]::GetFolderPath('UserProfile')
    $bundledRoot = Join-Path $userProfile '.cache\codex-runtimes'
    if (Test-Path -LiteralPath $bundledRoot -PathType Container) {
        $candidates = @(Get-ChildItem -LiteralPath $bundledRoot -Filter python.exe `
            -File -Recurse -ErrorAction SilentlyContinue | ForEach-Object FullName)
        foreach ($candidate in $candidates) {
            if (Test-PythonModule $candidate 'numpy') { return $candidate }
        }
    }
    throw 'The fixed FFT audit requires a local Python interpreter with NumPy.'
}

function Import-LocalMsvcEnvironment {
    if ($env:INCLUDE -and (Get-Command cl.exe -ErrorAction SilentlyContinue)) { return }

    $vswhereCandidates = @(
        (Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'),
        (Join-Path $env:ProgramFiles 'Microsoft Visual Studio\Installer\vswhere.exe')
    ) | Where-Object { $_ -and (Test-Path -LiteralPath $_ -PathType Leaf) }
    $vswhere = $vswhereCandidates | Select-Object -First 1
    if (-not $vswhere) { throw 'vswhere.exe is required to initialise local MSVC QA.' }

    $vsInstall = (& $vswhere -latest -products * `
        -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
        -property installationPath | Select-Object -First 1)
    if (-not $vsInstall) { throw 'A local Visual Studio C++ workload is required for compiled QA.' }
    $vsDevCmd = Join-Path $vsInstall.Trim() 'Common7\Tools\VsDevCmd.bat'
    if (-not (Test-Path -LiteralPath $vsDevCmd -PathType Leaf)) {
        throw "VsDevCmd.bat was not found: $vsDevCmd"
    }

    $environmentLines = & $env:ComSpec /d /s /c `
        "`"$vsDevCmd`" -no_logo -arch=x64 -host_arch=x64 && set"
    if ($LASTEXITCODE -ne 0) { throw 'Unable to initialise the local MSVC environment.' }
    foreach ($line in $environmentLines) {
        if ($line -match '^([^=]+)=(.*)$') {
            [Environment]::SetEnvironmentVariable($Matches[1], $Matches[2], 'Process')
        }
    }
}

if (-not $SkipBuild) {
    $buildArgs = @{
        Configuration = $Configuration
        BuildDir = $BuildDir
        Generator = 'Ninja'
    }
    if ($Clean) { $buildArgs.Clean = $true }
    & (Join-Path $PSScriptRoot 'build-live-windows.ps1') @buildArgs
}

$audits = @(
    'qa/audit_p0_sc220_device_detection.py',
    'qa/audit_sc220_real_stream_endpoints.py',
    'qa/audit_sc220_static_noise_clean.py',
    'qa/audit_sc220_runtime_regression_guard.py',
    'qa/audit_sc220_runtime_clock_status.py',
    'qa/audit_sc220_p3_live_ux.py',
    'qa/audit_sc220_p4_smart_ducking.py',
    'qa/audit_sc220_p5_console_polish.py',
    'qa/audit_sc220_p6a_ui_polish.py',
    'qa/audit_sc220_p6b_noise_cleanup.py',
    'qa/audit_sc220_p6c_status_chips.py',
    'qa/audit_sc220_p6d_tray_polish.py',
    'qa/audit_sc220_p6e_tray_trial_license.py',
    'qa/audit_sc220_p6f_native_tray_icon.py',
    'qa/audit_v0150_askp_p03_port.py',
    'qa/audit_sc220_dual_source_bus.py',
    'qa/audit_sc220_premium_ux.py',
    'qa/audit_sc220_console_alignment.py',
    'qa/audit_sc220_compact_pleasant.py',
    'qa/audit_sc220_premium_alignment.py',
    'qa/audit_sc220_digital_console_style.py',
    'qa/audit_sc220_p10_annotated_refinement.py',
    'qa/audit_sc220_monitor_bus.py',
    'qa/audit_sc220_pc_endpoint_routing.py',
    'qa/audit_sc220_public_release.py',
    'qa/audit_sc220_installer_bundle.py',
    'qa/audit_sc220_local_release.py',
    'qa/audit_sc220_clean_build.py'
)

foreach ($audit in $audits) {
    Write-Host "==> $audit" -ForegroundColor DarkCyan
    & $python (Join-Path $root $audit)
    if ($LASTEXITCODE -ne 0) { throw "Local source audit failed: $audit" }
}

# The historical FFT verifier writes a measurement report. Preserve the tracked
# reference byte-for-byte so routine QA cannot dirty the release checkout.
$fftReference = Join-Path $root 'qa/fixed_fft_reference_v0.8.2.txt'
$fftBytes = [IO.File]::ReadAllBytes($fftReference)
$fftPython = Find-NumpyPython $python
try {
    & $fftPython (Join-Path $root 'qa/verify_fixed_fft.py')
    if ($LASTEXITCODE -ne 0) { throw 'Fixed FFT verification failed.' }
}
finally {
    [IO.File]::WriteAllBytes($fftReference, $fftBytes)
}

$buildPath = Join-Path $root $BuildDir
Import-LocalMsvcEnvironment
& cmake --build $buildPath --config $Configuration --target `
    SC220LoudnessAudit SC220ProgrammeAnalyzerAudit SC220PleasantPresenceAudit
if ($LASTEXITCODE -ne 0) { throw 'Compiled local audit targets failed to build.' }

& ctest --test-dir $buildPath -C $Configuration --output-on-failure
if ($LASTEXITCODE -ne 0) { throw 'Compiled local CTest suite failed.' }

& git diff --check
if ($LASTEXITCODE -ne 0) { throw 'git diff --check failed after local QA.' }

Write-Host "`nSC220 LOCAL QA PASSED" -ForegroundColor Green
Write-Host "Build       : $BuildDir / $Configuration"
Write-Host "Python gates: $($audits.Count) + fixed FFT"
Write-Host "Compiled QA : SC220LoudnessAudit, SC220ProgrammeAnalyzerAudit, SC220PleasantPresenceAudit"
