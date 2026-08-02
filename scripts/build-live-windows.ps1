param(
    [ValidateSet('Release','Debug')]
    [string]$Configuration = 'Release',

    [string]$BuildDir = 'build-live',
    [string]$JuceSourceDir = '',

    [ValidateSet('Auto','VisualStudio','Ninja')]
    [string]$Generator = 'Auto',

    [switch]$ParityQa,
    [switch]$Clean
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version 2.0

$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$build = Join-Path $root $BuildDir

function Get-VsWherePath {
    $candidates = @(
        (Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'),
        (Join-Path $env:ProgramFiles 'Microsoft Visual Studio\Installer\vswhere.exe')
    ) | Where-Object { $_ -and (Test-Path $_) }

    return $candidates | Select-Object -First 1
}

function Get-VisualStudioInstallation {
    $vswhere = Get-VsWherePath
    if ($vswhere) {
        $installationPath = (& $vswhere -latest -products * `
            -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
            -property installationPath | Select-Object -First 1)
        $installationVersion = (& $vswhere -latest -products * `
            -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
            -property installationVersion | Select-Object -First 1)

        if ($installationPath) {
            return [pscustomobject]@{
                Path = $installationPath.Trim()
                Version = if ($installationVersion) { $installationVersion.Trim() } else { '0.0' }
                VsWhere = $vswhere
            }
        }

        $anyVisualStudio = (& $vswhere -latest -products * -property installationPath |
            Select-Object -First 1)
        if ($anyVisualStudio) {
            throw @"
Visual Studio was found, but the MSVC C++ toolchain is missing.
Open Visual Studio Installer -> Modify -> install:
  Desktop development with C++
Also keep these individual components enabled:
  MSVC v145 x64/x86 build tools, Windows SDK, C++ CMake tools for Windows.
"@
        }
    }

    # Fallback for unusual/offline Visual Studio installations where vswhere is unavailable.
    $vsRoot = Join-Path $env:ProgramFiles 'Microsoft Visual Studio'
    if (Test-Path $vsRoot) {
        $fallbacks = Get-ChildItem $vsRoot -Directory -ErrorAction SilentlyContinue |
            Sort-Object Name -Descending |
            ForEach-Object {
                Get-ChildItem $_.FullName -Directory -ErrorAction SilentlyContinue |
                    Where-Object {
                        Test-Path (Join-Path $_.FullName 'Common7\Tools\VsDevCmd.bat')
                    }
            }

        $fallback = $fallbacks | Select-Object -First 1
        if ($fallback) {
            $major = if ($fallback.Parent.Name -match '^2026$') { '18.0' }
                     elseif ($fallback.Parent.Name -match '^2022$') { '17.0' }
                     else { '0.0' }
            return [pscustomobject]@{
                Path = $fallback.FullName
                Version = $major
                VsWhere = $null
            }
        }
    }

    throw @"
No Visual Studio installation with the MSVC C++ workload was found.
Install Visual Studio Community 2026 and select 'Desktop development with C++'.
"@
}

function Get-CMakeInfo([string]$vsInstallPath) {
    $candidatePaths = [System.Collections.Generic.List[string]]::new()

    # Prefer the CMake bundled with the newest Visual Studio. VS 2026 normally
    # ships a CMake new enough to understand the Visual Studio 18 generator.
    $vsCMake = Join-Path $vsInstallPath 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
    if (Test-Path $vsCMake) { $candidatePaths.Add($vsCMake) }

    $pathCMake = Get-Command cmake.exe -ErrorAction SilentlyContinue
    if (-not $pathCMake) { $pathCMake = Get-Command cmake -ErrorAction SilentlyContinue }
    if ($pathCMake -and $pathCMake.Source) { $candidatePaths.Add($pathCMake.Source) }

    $infos = foreach ($path in ($candidatePaths | Select-Object -Unique)) {
        try {
            $versionText = (& $path --version 2>&1 | Out-String)
            if ($versionText -match 'cmake version\s+([0-9]+(?:\.[0-9]+){1,3})') {
                [pscustomobject]@{
                    Path = $path
                    Version = [version]$matches[1]
                }
            }
        } catch {
            Write-Verbose "CMake candidate failed: $path"
        }
    }

    $selected = $infos | Sort-Object Version -Descending | Select-Object -First 1
    if (-not $selected) {
        throw 'CMake was not found. Install C++ CMake tools for Windows from Visual Studio Installer.'
    }
    return $selected
}

function Import-VsDeveloperEnvironment([string]$vsInstallPath) {
    $vsDevCmd = Join-Path $vsInstallPath 'Common7\Tools\VsDevCmd.bat'
    if (-not (Test-Path $vsDevCmd)) {
        throw "VsDevCmd.bat was not found: $vsDevCmd"
    }

    Write-Host '==> Initialising the Visual Studio x64 developer environment'
    $command = "`"$vsDevCmd`" -no_logo -arch=x64 -host_arch=x64 && set"
    $environmentLines = & $env:ComSpec /d /s /c $command
    if ($LASTEXITCODE -ne 0) {
        throw "Visual Studio developer environment initialisation failed with exit code $LASTEXITCODE"
    }

    foreach ($line in $environmentLines) {
        if ($line -match '^([^=]+)=(.*)$') {
            [Environment]::SetEnvironmentVariable($matches[1], $matches[2], 'Process')
        }
    }

    if (-not (Get-Command cl.exe -ErrorAction SilentlyContinue)) {
        throw 'MSVC cl.exe is not available after running VsDevCmd.bat. Repair the C++ workload.'
    }
}

function Find-Ninja([string]$vsInstallPath) {
    $fromPath = Get-Command ninja.exe -ErrorAction SilentlyContinue
    if (-not $fromPath) { $fromPath = Get-Command ninja -ErrorAction SilentlyContinue }
    if ($fromPath -and $fromPath.Source) { return $fromPath.Source }

    $vsNinja = Join-Path $vsInstallPath 'Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe'
    if (Test-Path $vsNinja) { return $vsNinja }

    return $null
}

function Stop-SonkupikLiveProcesses {
    $stopped = 0
    try {
        $processes = Get-CimInstance Win32_Process -Filter "Name='Sonkupik Live.exe'" -ErrorAction SilentlyContinue
        foreach ($process in $processes) {
            try {
                Write-Host "==> Closing running Sonkupik Live process (PID $($process.ProcessId))" -ForegroundColor Yellow
                Stop-Process -Id ([int]$process.ProcessId) -Force -ErrorAction Stop
                $stopped++
            } catch {
                Write-Warning "Unable to close Sonkupik Live PID $($process.ProcessId): $($_.Exception.Message)"
            }
        }
    } catch {
        # CIM can be unavailable on stripped-down Windows environments. Fall
        # back to Get-Process, whose ProcessName omits the .exe suffix.
        Get-Process -Name 'Sonkupik Live' -ErrorAction SilentlyContinue | ForEach-Object {
            try {
                Write-Host "==> Closing running Sonkupik Live process (PID $($_.Id))" -ForegroundColor Yellow
                Stop-Process -Id $_.Id -Force -ErrorAction Stop
                $stopped++
            } catch {
                Write-Warning "Unable to close Sonkupik Live PID $($_.Id): $($_.Exception.Message)"
            }
        }
    }

    if ($stopped -gt 0) {
        Start-Sleep -Milliseconds 700
    }
    return $stopped
}

function Get-SonkupikOutputCandidates([string]$buildPath, [string]$configuration) {
    return @(
        (Join-Path $buildPath "SonkupikLive_artefacts\$configuration\Sonkupik Live.exe"),
        (Join-Path $buildPath "SonkupikLive_artefacts\$configuration\SonkupikLive.exe"),
        (Join-Path $buildPath 'SonkupikLive_artefacts\Sonkupik Live.exe'),
        (Join-Path $buildPath 'SonkupikLive_artefacts\SonkupikLive.exe')
    )
}

function Remove-StaleSonkupikOutputs([string]$buildPath, [string]$configuration) {
    foreach ($candidate in (Get-SonkupikOutputCandidates $buildPath $configuration)) {
        if (-not (Test-Path $candidate)) { continue }

        $removed = $false
        for ($attempt = 1; $attempt -le 6; $attempt++) {
            try {
                Remove-Item -LiteralPath $candidate -Force -ErrorAction Stop
                $removed = $true
                break
            } catch {
                if ($attempt -lt 6) {
                    Write-Host "==> Output file is busy; waiting before linker retry ($attempt/6)" -ForegroundColor Yellow
                    Start-Sleep -Milliseconds (350 * $attempt)
                } else {
                    throw @"
The previous Sonkupik Live executable is still locked:
  $candidate

Close Sonkupik Live, disable any Explorer preview for this file, and allow the
build folder in antivirus/Windows Security if it is being scanned continuously.
Then run:
  build-live-windows.bat clean
"@
                }
            }
        }

        if ($removed) {
            Write-Host "==> Removed stale executable: $candidate"
        }
    }
}

function Remove-IncompatibleBuildTree([string]$buildPath, [string]$wantedGenerator) {
    $cache = Join-Path $buildPath 'CMakeCache.txt'
    if (-not (Test-Path $cache)) { return }

    $cachedGeneratorLine = Select-String -Path $cache -Pattern '^CMAKE_GENERATOR:INTERNAL=(.+)$' |
        Select-Object -First 1
    $cachedGenerator = if ($cachedGeneratorLine) { $cachedGeneratorLine.Matches[0].Groups[1].Value } else { '' }

    if ($cachedGenerator -and $cachedGenerator -ne $wantedGenerator) {
        Write-Host "==> Generator changed: '$cachedGenerator' -> '$wantedGenerator'"
        Write-Host "==> Removing incompatible build tree: $buildPath"
        Remove-Item -Recurse -Force $buildPath
    }
}

$pinnedEngine = Join-Path $root 'third_party\askp-vst\engine\arsonkupik_dsp.cpp'
if (-not (Test-Path $pinnedEngine)) {
    throw "Pinned ArSonKuPik v0.5.23 authority is missing: $pinnedEngine"
}
Write-Host '==> Enhancer authority: ArSonKuPik v0.5.23 (pinned private submodule)'

$vs = Get-VisualStudioInstallation
$vsMajor = 0
if ($vs.Version -match '^(\d+)') { $vsMajor = [int]$matches[1] }

$cmake = Get-CMakeInfo $vs.Path
$cmakeHelp = (& $cmake.Path --help 2>&1 | Out-String)
$supportsVs2026 = $cmakeHelp.Contains('Visual Studio 18 2026')
$supportsVs2022 = $cmakeHelp.Contains('Visual Studio 17 2022')

Write-Host "==> Visual Studio: $($vs.Path) (version $($vs.Version))"
Write-Host "==> CMake: $($cmake.Path) (version $($cmake.Version))"

$selectedGenerator = $null
$useMultiConfig = $false
$ninjaPath = $null

if ($Generator -eq 'VisualStudio') {
    if ($vsMajor -ge 18) {
        if (-not $supportsVs2026) {
            throw @"
The selected CMake $($cmake.Version) does not support Visual Studio 2026.
The native 'Visual Studio 18 2026' generator requires CMake 4.2 or newer.
Update CMake, install 'C++ CMake tools for Windows' in Visual Studio Installer,
or run this script with -Generator Ninja.
"@
        }
        $selectedGenerator = 'Visual Studio 18 2026'
    } elseif ($vsMajor -eq 17 -and $supportsVs2022) {
        $selectedGenerator = 'Visual Studio 17 2022'
    } else {
        throw "No supported Visual Studio CMake generator matches Visual Studio version $($vs.Version)."
    }
    $useMultiConfig = $true
}
elseif ($Generator -eq 'Ninja') {
    Import-VsDeveloperEnvironment $vs.Path
    $ninjaPath = Find-Ninja $vs.Path
    if (-not $ninjaPath) {
        throw "Ninja was not found. Install 'C++ CMake tools for Windows' in Visual Studio Installer."
    }
    $selectedGenerator = 'Ninja'
}
else {
    # Auto: prefer native Visual Studio projects when the chosen CMake supports
    # the installed IDE. Otherwise use Ninja with the same MSVC compiler.
    if ($vsMajor -ge 18 -and $supportsVs2026) {
        $selectedGenerator = 'Visual Studio 18 2026'
        $useMultiConfig = $true
    }
    elseif ($vsMajor -eq 17 -and $supportsVs2022) {
        $selectedGenerator = 'Visual Studio 17 2022'
        $useMultiConfig = $true
    }
    else {
        Write-Host '==> Native Visual Studio generator is unavailable in this CMake; falling back to Ninja + MSVC.' -ForegroundColor Yellow
        Import-VsDeveloperEnvironment $vs.Path
        $ninjaPath = Find-Ninja $vs.Path
        if (-not $ninjaPath) {
            throw @"
Neither a matching Visual Studio generator nor Ninja is available.
For Visual Studio 2026, install CMake 4.2+ or enable 'C++ CMake tools for Windows'.
"@
        }
        $selectedGenerator = 'Ninja'
    }
}

if ($Clean -and (Test-Path $build)) {
    Write-Host "==> Removing $build"
    Remove-Item -Recurse -Force $build
}

Remove-IncompatibleBuildTree $build $selectedGenerator

$cmakeArgs = @(
    '-S', $root,
    '-B', $build,
    '-G', $selectedGenerator
)

if ($ParityQa) {
    $cmakeArgs += '-DSONKUPIK_BUILD_PARITY_QA=ON'
    Write-Host '==> P2 parity QA renderers enabled'
}

if ($useMultiConfig) {
    $cmakeArgs += @('-A', 'x64', "-DCMAKE_GENERATOR_INSTANCE=$($vs.Path)")
} else {
    $cmakeArgs += "-DCMAKE_BUILD_TYPE=$Configuration"
    if ($ninjaPath) { $cmakeArgs += "-DCMAKE_MAKE_PROGRAM=$ninjaPath" }
}

if ($JuceSourceDir) {
    $resolvedJuce = (Resolve-Path $JuceSourceDir).Path
    $cmakeArgs += "-DJUCE_SOURCE_DIR=$resolvedJuce"
    Write-Host "==> Using local JUCE: $resolvedJuce"
} elseif ($env:JUCE_DIR -and (Test-Path $env:JUCE_DIR)) {
    $resolvedJuce = (Resolve-Path $env:JUCE_DIR).Path
    $cmakeArgs += "-DJUCE_SOURCE_DIR=$resolvedJuce"
    Write-Host "==> Using JUCE_DIR: $resolvedJuce"
} elseif (Test-Path (Join-Path $root 'third_party\JUCE\CMakeLists.txt')) {
    $resolvedJuce = (Resolve-Path (Join-Path $root 'third_party\JUCE')).Path
    $cmakeArgs += "-DJUCE_SOURCE_DIR=$resolvedJuce"
    Write-Host "==> Using vendored/local JUCE: $resolvedJuce"
} else {
    Write-Host '==> JUCE will be fetched by CMake.'
    Write-Host '    For offline/reliable builds use -JuceSourceDir, JUCE_DIR, or third_party\JUCE.'
}

Write-Host "==> Generator: $selectedGenerator"
Write-Host '==> Configuring Sonkupik Live'
& $cmake.Path @cmakeArgs
if ($LASTEXITCODE -ne 0) { throw "CMake configure failed with exit code $LASTEXITCODE" }

# The linker cannot replace an executable that is still running. Close any
# previous preview instance and remove the stale output before invoking MSBuild.
Stop-SonkupikLiveProcesses | Out-Null
Remove-StaleSonkupikOutputs $build $Configuration

Write-Host "==> Building Sonkupik Live ($Configuration)"
$buildTargets = @('SonkupikLive')
if ($ParityQa) { $buildTargets += @('SonkupikParityReference', 'SonkupikParityLive') }
$buildArgs = @('--build', $build, '--target') + $buildTargets + @('--parallel')
if ($useMultiConfig) { $buildArgs += @('--config', $Configuration) }

$buildLog = Join-Path $build 'sonkupik-build-last.log'
& $cmake.Path @buildArgs 2>&1 | Tee-Object -FilePath $buildLog
$firstBuildExitCode = $LASTEXITCODE
if ($firstBuildExitCode -ne 0) {
    $buildText = if (Test-Path $buildLog) { Get-Content -Raw $buildLog } else { '' }
    $isExecutableLock = $buildText -match 'LNK1104' -and
                        $buildText -match 'Sonkupik(?: Live|Live).*\.exe'

    if (-not $isExecutableLock) {
        throw "Build failed with exit code $firstBuildExitCode. This is a compile/source error; linker-lock retry was skipped."
    }

    Write-Host '==> Linker reported a locked Sonkupik Live executable. Releasing it and retrying once.' -ForegroundColor Yellow
    Stop-SonkupikLiveProcesses | Out-Null
    Remove-StaleSonkupikOutputs $build $Configuration
    Start-Sleep -Milliseconds 900
    & $cmake.Path @buildArgs 2>&1 | Tee-Object -FilePath $buildLog
}
if ($LASTEXITCODE -ne 0) {
    throw "Build failed with exit code $LASTEXITCODE after the executable-lock retry"
}

$candidates = Get-SonkupikOutputCandidates $build $Configuration
$exe = $candidates | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $exe) {
    $exe = Get-ChildItem $build -Recurse -Filter '*.exe' |
        Where-Object { $_.Name -match '(Sonkupik.*Live)' -and $_.Name -notmatch '(Parity|Renderer)' } |
        Select-Object -First 1 -ExpandProperty FullName
}
if (-not $exe) { throw 'Build completed but Sonkupik Live executable was not found.' }

Write-Host "`nSUCCESS: $exe" -ForegroundColor Green
