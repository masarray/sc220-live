$ErrorActionPreference = 'Stop'
Set-StrictMode -Version 2.0

$machinePath = [Environment]::GetEnvironmentVariable('Path', 'Machine')
if ([string]::IsNullOrWhiteSpace($machinePath)) {
    throw 'The Windows machine PATH is empty.'
}

# The runner is installed as a Windows service. Verify against machine-level
# paths rather than relying on the interactive user's PATH.
$env:Path = $machinePath

$missing = [System.Collections.Generic.List[string]]::new()
foreach ($command in @('git.exe', 'gh.exe', 'cmake.exe', 'ninja.exe')) {
    if (-not (Get-Command $command -ErrorAction SilentlyContinue)) {
        $missing.Add($command)
    }
}
if (-not (Get-Command py.exe -ErrorAction SilentlyContinue) -and
    -not (Get-Command python.exe -ErrorAction SilentlyContinue)) {
    $missing.Add('Python (py.exe or python.exe)')
}

$inno = @(
    (Join-Path ${env:ProgramFiles(x86)} 'Inno Setup 6\ISCC.exe'),
    (Join-Path $env:ProgramFiles 'Inno Setup 6\ISCC.exe')
) | Where-Object { $_ -and (Test-Path -LiteralPath $_ -PathType Leaf) } |
    Select-Object -First 1
if (-not $inno) { $missing.Add('Inno Setup 6 (ISCC.exe)') }

$vswhere = @(
    (Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'),
    (Join-Path $env:ProgramFiles 'Microsoft Visual Studio\Installer\vswhere.exe')
) | Where-Object { $_ -and (Test-Path -LiteralPath $_ -PathType Leaf) } |
    Select-Object -First 1
$cpp = $null
if ($vswhere) {
    $cpp = (& $vswhere -latest -products * `
        -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
        -property installationPath | Select-Object -First 1)
}
if (-not $cpp) { $missing.Add('Visual Studio C++ Build Tools') }

$runner = Get-Service -Name 'actions.runner.*' -ErrorAction SilentlyContinue |
    Where-Object { $_.Name -match '(?i)sc220|masarray' } |
    Select-Object -First 1
if (-not $runner) {
    $missing.Add('SC220 GitHub Actions runner service')
} elseif ($runner.Status -ne 'Running') {
    try {
        Start-Service -Name $runner.Name
        $runner = Get-Service -Name $runner.Name
    } catch {
        $missing.Add("SC220 runner service Running state ($($runner.Name))")
    }
}

if ($missing.Count -gt 0) {
    if ($runner -and $runner.Status -eq 'Running') {
        Stop-Service -Name $runner.Name -Force -ErrorAction SilentlyContinue
    }
    throw "Local runner host is incomplete:`n - $($missing -join "`n - ")`nRe-run SETUP-LOCAL-RUNNER.cmd after correcting the listed component(s)."
}

Write-Host 'SC220 LOCAL RUNNER HOST READY' -ForegroundColor Green
Write-Host "Runner : $($runner.Name) [$($runner.Status)]"
Write-Host "MSVC   : $($cpp.Trim())"
Write-Host "Inno   : $inno"
Write-Host "Ninja  : $((Get-Command ninja.exe).Source)"
