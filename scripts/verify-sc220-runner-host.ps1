$ErrorActionPreference = 'Stop'
Set-StrictMode -Version 2.0

$runnerRoot = 'C:\actions-runner-sc220'
$runnerEnvPath = Join-Path $runnerRoot '.env'

$machinePath = [Environment]::GetEnvironmentVariable('Path', 'Machine')
if ([string]::IsNullOrWhiteSpace($machinePath)) {
    throw 'The Windows machine PATH is empty.'
}

# The runner is a Windows service. Verify against machine-level paths rather
# than relying on the interactive user's PATH.
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

if (-not (Test-Path -LiteralPath $runnerEnvPath -PathType Leaf)) {
    $missing.Add("Runner environment file ($runnerEnvPath)")
} else {
    $runnerEnv = @(Get-Content -LiteralPath $runnerEnvPath)
    if ($runnerEnv -notcontains 'PSExecutionPolicyPreference=Bypass') {
        $missing.Add('Runner-scoped PowerShell execution policy Bypass')
    }
}

$runner = Get-Service -Name 'actions.runner.*' -ErrorAction SilentlyContinue |
    Where-Object { $_.Name -match '(?i)sc220|masarray' } |
    Select-Object -First 1
$runnerInfo = $null
if (-not $runner) {
    $missing.Add('SC220 GitHub Actions runner service')
} else {
    $runnerInfo = Get-CimInstance Win32_Service -Filter "Name='$($runner.Name)'"
    if (-not $runnerInfo) {
        $missing.Add('SC220 runner service configuration')
    } else {
        if ($runnerInfo.StartName -notin @('LocalSystem', 'NT AUTHORITY\SYSTEM')) {
            $missing.Add("LocalSystem runner account (actual: $($runnerInfo.StartName))")
        }
        if ($runnerInfo.StartMode -ne 'Manual') {
            $missing.Add("Manual runner startup mode (actual: $($runnerInfo.StartMode))")
        }
    }

    if ($runner.Status -ne 'Running') {
        try {
            Start-Service -Name $runner.Name
            $runner = Get-Service -Name $runner.Name
            $runner.WaitForStatus('Running', [TimeSpan]::FromSeconds(30))
        } catch {
            $missing.Add("SC220 runner service Running state ($($runner.Name))")
        }
    }
}

try {
    if ($missing.Count -gt 0) {
        throw "Local runner host is incomplete:`n - $($missing -join "`n - ")`nRe-run SETUP-LOCAL-RUNNER.cmd after correcting the listed component(s)."
    }

    Write-Host 'SC220 LOCAL RUNNER HOST READY' -ForegroundColor Green
    Write-Host "Runner : $($runner.Name) [$($runner.Status)]"
    Write-Host "Account: $($runnerInfo.StartName)"
    Write-Host 'Startup: Manual / release-only'
    Write-Host 'PowerShell: runner-scoped Bypass'
    Write-Host "MSVC   : $($cpp.Trim())"
    Write-Host "Inno   : $inno"
    Write-Host "Ninja  : $((Get-Command ninja.exe).Source)"
}
finally {
    if ($runner -and (Get-Service -Name $runner.Name).Status -ne 'Stopped') {
        Stop-Service -Name $runner.Name -Force -ErrorAction SilentlyContinue
    }
}
