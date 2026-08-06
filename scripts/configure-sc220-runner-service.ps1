param(
    [string]$RunnerRoot = 'C:\actions-runner-sc220'
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version 2.0

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

# Windows services may retain the machine environment that existed before this
# setup installed build tools. The runner reads .env at startup, so pin the
# current machine-level PATH without relying on a reboot or the user's PATH.
$machinePath = [Environment]::GetEnvironmentVariable('Path', 'Machine')
if ([string]::IsNullOrWhiteSpace($machinePath)) {
    throw 'The Windows machine PATH is empty.'
}
$envText = "PATH=$machinePath`nSC220_RELEASE_RUNNER=1`n"
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
