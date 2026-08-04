param(
    [string]$BuildDir = 'build-sc220',
    [int]$MaxAttempts = 8
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version 2.0

$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$build = [System.IO.Path]::GetFullPath((Join-Path $root $BuildDir))
$buildPrefix = $build.TrimEnd('\') + '\'

function Test-IsBuildExecutable([string]$path) {
    if ([string]::IsNullOrWhiteSpace($path)) { return $false }
    try {
        $fullPath = [System.IO.Path]::GetFullPath($path)
        return $fullPath.StartsWith($buildPrefix, [System.StringComparison]::OrdinalIgnoreCase)
    } catch {
        return $false
    }
}

function Stop-BuildSC220Processes {
    $stopped = 0
    $candidateNames = @('SC220 Live.exe', 'SC220Live.exe')

    try {
        $processes = Get-CimInstance Win32_Process -ErrorAction SilentlyContinue |
            Where-Object { $candidateNames -contains $_.Name }

        foreach ($process in $processes) {
            if (-not (Test-IsBuildExecutable $process.ExecutablePath)) { continue }

            Write-Host "==> Closing build preview SC220 Live (PID $($process.ProcessId))" -ForegroundColor Yellow
            Stop-Process -Id ([int]$process.ProcessId) -Force -ErrorAction Stop
            $stopped++
        }
    } catch {
        Write-Verbose "CIM process inspection failed: $($_.Exception.Message)"
    }

    # Get-Process is a fallback for hosts where Win32_Process is restricted.
    foreach ($name in @('SC220 Live', 'SC220Live')) {
        Get-Process -Name $name -ErrorAction SilentlyContinue | ForEach-Object {
            $path = $null
            try { $path = $_.Path } catch { }
            if (-not (Test-IsBuildExecutable $path)) { return }

            try {
                Write-Host "==> Closing build preview SC220 Live (PID $($_.Id))" -ForegroundColor Yellow
                Stop-Process -Id $_.Id -Force -ErrorAction Stop
                $stopped++
            } catch {
                Write-Warning "Unable to close SC220 Live PID $($_.Id): $($_.Exception.Message)"
            }
        }
    }

    if ($stopped -gt 0) {
        Start-Sleep -Milliseconds 800
    }

    return $stopped
}

function Get-LikelyLockOwners {
    try {
        return Get-CimInstance Win32_Process -ErrorAction SilentlyContinue |
            Where-Object {
                $_.Name -in @('SC220 Live.exe', 'SC220Live.exe') -or
                (Test-IsBuildExecutable $_.ExecutablePath)
            } |
            ForEach-Object {
                "PID $($_.ProcessId): $($_.Name) [$($_.ExecutablePath)]"
            }
    } catch {
        return @()
    }
}

if (-not (Test-Path -LiteralPath $build)) {
    return
}

Stop-BuildSC220Processes | Out-Null

for ($attempt = 1; $attempt -le $MaxAttempts; $attempt++) {
    try {
        Write-Host "==> Removing $build"
        Remove-Item -LiteralPath $build -Recurse -Force -ErrorAction Stop
        return
    } catch {
        if ($attempt -lt $MaxAttempts) {
            Write-Host "==> Build tree is still busy; retrying ($attempt/$MaxAttempts)" -ForegroundColor Yellow
            Stop-BuildSC220Processes | Out-Null
            Start-Sleep -Milliseconds (300 * $attempt)
            continue
        }

        $owners = @(Get-LikelyLockOwners)
        $ownerText = if ($owners.Count -gt 0) {
            "`nLikely lock owner(s):`n  " + ($owners -join "`n  ")
        } else {
            "`nNo SC220 process was visible. Explorer preview, antivirus, or an indexing service may still hold the file."
        }

        throw @"
Unable to remove the SC220 build tree after $MaxAttempts attempts:
  $build
$ownerText

Close SC220 Live completely, including its tray icon. Also close any Explorer
window previewing the executable, then run the clean build again.
"@
    }
}
