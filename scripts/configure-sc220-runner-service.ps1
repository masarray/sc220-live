param(
    [string]$RunnerRoot = 'C:\actions-runner-sc220'
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version 2.0

function Get-MachineCommand([string]$Name) {
    $savedPath = $env:Path
    try {
        $env:Path = [Environment]::GetEnvironmentVariable('Path', 'Machine')
        return Get-Command $Name -ErrorAction SilentlyContinue
    }
    finally {
        $env:Path = $savedPath
    }
}

function Resolve-PythonWingetId {
    foreach ($candidate in @('Python.Python.3.13', 'Python.Python.3.12')) {
        & winget.exe show --id $candidate --exact --accept-source-agreements --disable-interactivity *> $null
        if ($LASTEXITCODE -eq 0) { return $candidate }
    }
    return $null
}

function Ensure-MachinePython {
    $python = Get-MachineCommand 'py.exe'
    if (-not $python) { $python = Get-MachineCommand 'python.exe' }
    if ($python) {
        Write-Host "Machine Python : $($python.Source)" -ForegroundColor Green
        return
    }

    Write-Host 'Machine-wide Python is missing; installing it for the runner...' -ForegroundColor Yellow
    if (-not (Get-Command winget.exe -ErrorAction SilentlyContinue)) {
        throw 'winget.exe is required to install machine-wide Python.'
    }

    $pythonId = Resolve-PythonWingetId
    if (-not $pythonId) {
        throw 'No supported official Python package was found in winget.'
    }

    & winget.exe install --id $pythonId --exact --scope machine --force `
        --accept-source-agreements --accept-package-agreements `
        --silent --disable-interactivity
    if ($LASTEXITCODE -ne 0) {
        Write-Warning "winget returned $LASTEXITCODE while installing $pythonId; verifying the machine installation before failing."
    }

    $python = Get-MachineCommand 'py.exe'
    if (-not $python) { $python = Get-MachineCommand 'python.exe' }

    if (-not $python) {
        $candidate = Get-ChildItem -LiteralPath $env:ProgramFiles -Directory -Filter 'Python*' -ErrorAction SilentlyContinue |
            ForEach-Object { Join-Path $_.FullName 'python.exe' } |
            Where-Object { Test-Path -LiteralPath $_ -PathType Leaf } |
            Sort-Object -Descending |
            Select-Object -First 1

        if ($candidate) {
            $pythonDir = Split-Path -Parent $candidate
            $machinePath = [Environment]::GetEnvironmentVariable('Path', 'Machine')
            $segments = @($machinePath -split ';' | Where-Object { $_ })
            if ($segments -notcontains $pythonDir) {
                $newMachinePath = (($segments + $pythonDir) -join ';')
                [Environment]::SetEnvironmentVariable('Path', $newMachinePath, 'Machine')
            }
            $python = Get-MachineCommand 'python.exe'
        }
    }

    if (-not $python) {
        throw 'Python was installed or repaired, but it is still not visible on the machine PATH used by the runner.'
    }

    & $python.Source --version
    if ($LASTEXITCODE -ne 0) {
        throw "Machine Python exists but failed to start: $($python.Source)"
    }
    Write-Host "Machine Python : $($python.Source)" -ForegroundColor Green
}

$service = Get-Service -Name 'actions.runner.*' -ErrorAction SilentlyContinue |
    Where-Object { $_.Name -match '(?i)sc220|masarray' } |
    Select-Object -First 1
if (-not $service) {
    throw 'SC220 GitHub Actions runner service was not found.'
}
if (-not (Test-Path -LiteralPath $RunnerRoot -PathType Container)) {
    throw "Runner folder was not found: $RunnerRoot"
}

if ($service.Status -ne 'Stopped') {
    Stop-Service -Name $service.Name -Force
    $service.WaitForStatus('Stopped', [TimeSpan]::FromSeconds(30))
}

# The release smoke test installs and uninstalls an admin-default Inno Setup
# package. Keep the high-privilege runner offline except during an explicit
# RELEASE-SC220.cmd session.
& sc.exe config $service.Name obj= LocalSystem start= demand | Out-Host
if ($LASTEXITCODE -ne 0) {
    throw "Unable to configure $($service.Name) as an on-demand LocalSystem service."
}
Set-Service -Name $service.Name -StartupType Manual

# The initial setup may have found a per-user Python that LocalSystem cannot use.
# Repair that edge case before snapshotting PATH for the runner service.
Ensure-MachinePython

# Windows services may retain the machine environment that existed before this
# setup installed build tools. The runner reads .env at startup, so pin the
# current machine-level PATH without relying on a reboot or the user's PATH.
# PSExecutionPolicyPreference applies only to PowerShell processes launched by
# this runner service. It avoids changing LocalMachine/CurrentUser policy while
# allowing GitHub Actions' generated temporary .ps1 scripts to execute.
$machinePath = [Environment]::GetEnvironmentVariable('Path', 'Machine')
if ([string]::IsNullOrWhiteSpace($machinePath)) {
    throw 'The Windows machine PATH is empty.'
}
$envText = "PATH=$machinePath`nPSExecutionPolicyPreference=Bypass`nSC220_RELEASE_RUNNER=1`n"
$utf8NoBom = [Text.UTF8Encoding]::new($false)
[IO.File]::WriteAllText((Join-Path $RunnerRoot '.env'), $envText, $utf8NoBom)

$serviceInfo = Get-CimInstance Win32_Service -Filter "Name='$($service.Name)'"
if (-not $serviceInfo) { throw 'Runner service configuration could not be read back.' }
if ($serviceInfo.StartName -notin @('LocalSystem', 'NT AUTHORITY\SYSTEM')) {
    throw "Unexpected runner service account: $($serviceInfo.StartName)"
}
if ($serviceInfo.StartMode -ne 'Manual') {
    throw "Unexpected runner startup mode: $($serviceInfo.StartMode)"
}

Write-Host 'SC220 RUNNER SERVICE SECURITY MODE READY' -ForegroundColor Green
Write-Host "Service : $($service.Name)"
Write-Host "Account : $($serviceInfo.StartName)"
Write-Host 'Startup : Manual (started only by RELEASE-SC220.cmd)'
Write-Host "PATH    : pinned in $RunnerRoot\.env"
Write-Host 'PowerShell policy: Bypass for runner child processes only'
