[CmdletBinding()]
param(
    [ValidateSet('Summary', 'Core', 'VB-Cable')]
    [string]$Mode = 'Summary',

    [string]$OutputPath = ''
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version 2.0

function Get-VCRuntimeState {
    $paths = @(
        'HKLM:\SOFTWARE\Microsoft\VisualStudio\14.0\VC\Runtimes\x64',
        'HKLM:\SOFTWARE\WOW6432Node\Microsoft\VisualStudio\14.0\VC\Runtimes\x64'
    )

    foreach ($path in $paths) {
        try {
            $value = Get-ItemProperty -LiteralPath $path -ErrorAction Stop
            if ([int]$value.Installed -eq 1) {
                return [ordered]@{
                    installed = $true
                    version = [string]$value.Version
                    registryPath = $path
                }
            }
        } catch {
            # Continue to the compatibility view.
        }
    }

    return [ordered]@{
        installed = $false
        version = ''
        registryPath = ''
    }
}

function Get-ServiceState([string]$Name) {
    try {
        $service = Get-Service -Name $Name -ErrorAction Stop
        return [ordered]@{
            found = $true
            running = ($service.Status -eq [System.ServiceProcess.ServiceControllerStatus]::Running)
            status = [string]$service.Status
        }
    } catch {
        return [ordered]@{
            found = $false
            running = $false
            status = 'NotFound'
        }
    }
}

function Get-VBCableState {
    $driverFound = $false
    $cableInputFound = $false
    $cableOutputFound = $false
    $driverNames = @()
    $endpointNames = @()
    $probeError = ''

    try {
        $allDevices = @(Get-PnpDevice -PresentOnly -ErrorAction Stop)
        $mediaDevices = @($allDevices | Where-Object {
            $_.Class -in @('MEDIA', 'AudioEndpoint') -or
            $_.FriendlyName -match '(?i)VB-Audio|VB-CABLE|VBCABLE'
        })

        foreach ($device in $mediaDevices) {
            $name = [string]$device.FriendlyName
            if ([string]::IsNullOrWhiteSpace($name)) {
                $name = [string]$device.InstanceId
            }

            if ($device.Class -eq 'AudioEndpoint') {
                $endpointNames += $name
                if ($name -match '(?i)\bCABLE Input\b') { $cableInputFound = $true }
                if ($name -match '(?i)\bCABLE Output\b') { $cableOutputFound = $true }
            }

            if ($name -match '(?i)VB-Audio|VB-CABLE|VBCABLE') {
                $driverFound = $true
                if ($device.Class -ne 'AudioEndpoint') {
                    $driverNames += $name
                }
            }
        }
    } catch {
        $probeError = $_.Exception.Message
    }

    $ready = $driverFound -and $cableInputFound -and $cableOutputFound
    $partial = (-not $ready) -and ($driverFound -or $cableInputFound -or $cableOutputFound)

    return [ordered]@{
        ready = $ready
        partial = $partial
        driverFound = $driverFound
        cableInputFound = $cableInputFound
        cableOutputFound = $cableOutputFound
        driverNames = @($driverNames | Sort-Object -Unique)
        endpointNames = @($endpointNames | Sort-Object -Unique)
        probeError = $probeError
    }
}

$vcRuntime = Get-VCRuntimeState
$windowsAudio = Get-ServiceState 'Audiosrv'
$audioEndpointBuilder = Get-ServiceState 'AudioEndpointBuilder'
$vbCable = Get-VBCableState

$result = [ordered]@{
    schema = 'sc220-prerequisite-probe-v1'
    generatedUtc = [DateTime]::UtcNow.ToString('o')
    operatingSystem = [ordered]@{
        caption = [Environment]::OSVersion.VersionString
        is64Bit = [Environment]::Is64BitOperatingSystem
    }
    vcRuntime = $vcRuntime
    services = [ordered]@{
        windowsAudio = $windowsAudio
        audioEndpointBuilder = $audioEndpointBuilder
    }
    vbCable = $vbCable
}

$json = $result | ConvertTo-Json -Depth 7
if (-not [string]::IsNullOrWhiteSpace($OutputPath)) {
    $parent = Split-Path -Parent $OutputPath
    if ($parent) {
        New-Item -ItemType Directory -Force -Path $parent | Out-Null
    }
    Set-Content -LiteralPath $OutputPath -Value $json -Encoding UTF8
} else {
    Write-Output $json
}

$coreReady = $vcRuntime.installed -and
             $windowsAudio.running -and
             $audioEndpointBuilder.running

switch ($Mode) {
    'Core' {
        if (-not $vcRuntime.installed) { exit 11 }
        if (-not $windowsAudio.running -or -not $audioEndpointBuilder.running) { exit 12 }
        exit 0
    }
    'VB-Cable' {
        if ($vbCable.ready) { exit 0 }
        if ($vbCable.partial) { exit 21 }
        exit 22
    }
    default {
        if ($coreReady -and $vbCable.ready) { exit 0 }
        exit 1
    }
}
