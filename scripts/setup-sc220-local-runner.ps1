param(
    [string]$Repository = 'masarray/sc220-live',
    [string]$RunnerRoot = 'C:\actions-runner-sc220',
    [string]$RunnerLabel = 'sc220-build'
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version 2.0

function Write-Section([string]$Title) {
    Write-Host "`n============================================================" -ForegroundColor DarkCyan
    Write-Host " $Title" -ForegroundColor Cyan
    Write-Host "============================================================" -ForegroundColor DarkCyan
}

function Test-Administrator {
    $identity = [Security.Principal.WindowsIdentity]::GetCurrent()
    $principal = [Security.Principal.WindowsPrincipal]::new($identity)
    return $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
}

function Restart-Elevated {
    $arguments = @(
        '-NoLogo',
        '-NoProfile',
        '-ExecutionPolicy', 'Bypass',
        '-File', ('"{0}"' -f $PSCommandPath),
        '-Repository', ('"{0}"' -f $Repository),
        '-RunnerRoot', ('"{0}"' -f $RunnerRoot),
        '-RunnerLabel', ('"{0}"' -f $RunnerLabel)
    ) -join ' '

    Start-Process powershell.exe -Verb RunAs -ArgumentList $arguments -Wait
}

function Refresh-ProcessPath {
    $machine = [Environment]::GetEnvironmentVariable('Path', 'Machine')
    $user = [Environment]::GetEnvironmentVariable('Path', 'User')
    $env:Path = (@($machine, $user) | Where-Object { $_ }) -join ';'
}

function Test-Command([string]$Name) {
    return $null -ne (Get-Command $Name -ErrorAction SilentlyContinue)
}

function Invoke-WingetInstall {
    param(
        [Parameter(Mandatory = $true)][string]$Id,
        [string]$Override = '',
        [switch]$MachineScope
    )

    $arguments = @(
        'install', '--id', $Id, '--exact',
        '--accept-source-agreements', '--accept-package-agreements',
        '--silent', '--disable-interactivity'
    )
    if ($MachineScope) { $arguments += @('--scope', 'machine') }
    if ($Override) { $arguments += @('--override', $Override) }

    Write-Host "==> winget $($arguments -join ' ')" -ForegroundColor DarkGray
    & winget.exe @arguments
    if ($LASTEXITCODE -ne 0) {
        Write-Warning "winget returned $LASTEXITCODE for $Id. The script will verify the installed command before failing."
    }
    Refresh-ProcessPath
}

function Find-VsWhere {
    return @(
        (Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'),
        (Join-Path $env:ProgramFiles 'Microsoft Visual Studio\Installer\vswhere.exe')
    ) | Where-Object { $_ -and (Test-Path -LiteralPath $_ -PathType Leaf) } |
        Select-Object -First 1
}

function Get-CppBuildTools {
    $vswhere = Find-VsWhere
    if (-not $vswhere) { return $null }

    $path = (& $vswhere -latest -products * `
        -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
        -property installationPath | Select-Object -First 1)
    if (-not $path) { return $null }

    $path = $path.Trim()
    $devCmd = Join-Path $path 'Common7\Tools\VsDevCmd.bat'
    if (-not (Test-Path -LiteralPath $devCmd -PathType Leaf)) { return $null }

    return [pscustomobject]@{
        InstallationPath = $path
        DevCmd = $devCmd
    }
}

function Resolve-AvailableWingetId([string[]]$Candidates) {
    foreach ($candidate in $Candidates) {
        & winget.exe show --id $candidate --exact --accept-source-agreements --disable-interactivity *> $null
        if ($LASTEXITCODE -eq 0) { return $candidate }
    }
    return $null
}

function Read-SecretText([string]$Prompt) {
    $secure = Read-Host $Prompt -AsSecureString
    $pointer = [Runtime.InteropServices.Marshal]::SecureStringToBSTR($secure)
    try {
        return [Runtime.InteropServices.Marshal]::PtrToStringBSTR($pointer)
    }
    finally {
        [Runtime.InteropServices.Marshal]::ZeroFreeBSTR($pointer)
    }
}

function Invoke-GhJson {
    param([Parameter(ValueFromRemainingArguments = $true)][string[]]$Arguments)
    $output = & gh.exe @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "GitHub CLI failed: gh $($Arguments -join ' ')"
    }
    if ([string]::IsNullOrWhiteSpace(($output | Out-String))) { return $null }
    return (($output | Out-String) | ConvertFrom-Json)
}

function Set-RepositorySecret([string]$Name, [string]$Value) {
    if ([string]::IsNullOrWhiteSpace($Value)) {
        throw "Secret $Name cannot be empty."
    }

    $Value | & gh.exe secret set $Name --repo $Repository --body -
    if ($LASTEXITCODE -ne 0) {
        throw "Unable to set repository secret $Name."
    }
}

function Get-RepositorySecretNames {
    $json = & gh.exe secret list --repo $Repository --json name
    if ($LASTEXITCODE -ne 0) {
        throw 'Unable to inspect repository secrets. Run gh auth refresh -h github.com -s repo,workflow and retry.'
    }
    return @($json | ConvertFrom-Json | ForEach-Object { $_.name })
}

function Install-RunnerApplication([string]$RegistrationToken) {
    Write-Section 'Mengunduh GitHub Actions Runner resmi'

    $headers = @{
        Accept = 'application/vnd.github+json'
        'User-Agent' = 'SC220-Local-Runner-Setup/1.0'
        'X-GitHub-Api-Version' = '2022-11-28'
    }
    $release = Invoke-RestMethod -Headers $headers `
        -Uri 'https://api.github.com/repos/actions/runner/releases/latest'
    $asset = @($release.assets) | Where-Object {
        $_.name -match '^actions-runner-win-x64-[0-9.]+\.zip$'
    } | Select-Object -First 1

    if (-not $asset) {
        throw 'The latest official actions/runner release has no Windows x64 ZIP asset.'
    }

    $archive = Join-Path $env:TEMP $asset.name
    Write-Host "Runner version : $($release.tag_name)"
    Write-Host "Download       : $($asset.name)"
    Invoke-WebRequest -UseBasicParsing -MaximumRedirection 5 `
        -Uri $asset.browser_download_url -OutFile $archive

    if ($asset.PSObject.Properties.Name -contains 'digest' -and
        $asset.digest -match '^sha256:([A-Fa-f0-9]{64})$') {
        $actual = (Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash.ToLowerInvariant()
        $expected = $Matches[1].ToLowerInvariant()
        if ($actual -ne $expected) {
            throw "GitHub runner SHA-256 mismatch. Expected $expected, actual $actual"
        }
        Write-Host 'Runner SHA-256 verified.' -ForegroundColor Green
    }

    if (Test-Path -LiteralPath $RunnerRoot) {
        Remove-Item -LiteralPath $RunnerRoot -Recurse -Force
    }
    New-Item -ItemType Directory -Path $RunnerRoot -Force | Out-Null
    Expand-Archive -LiteralPath $archive -DestinationPath $RunnerRoot -Force
    Remove-Item -LiteralPath $archive -Force -ErrorAction SilentlyContinue

    Push-Location $RunnerRoot
    try {
        & .\config.cmd --unattended `
            --url "https://github.com/$Repository" `
            --token $RegistrationToken `
            --name "$env:COMPUTERNAME-SC220" `
            --labels $RunnerLabel `
            --work '_work' `
            --runasservice `
            --replace
        if ($LASTEXITCODE -ne 0) {
            throw "GitHub runner configuration failed with exit code $LASTEXITCODE."
        }
    }
    finally {
        Pop-Location
    }

    $service = Get-Service -Name 'actions.runner.*' -ErrorAction SilentlyContinue |
        Where-Object { $_.Name -match 'sc220|masarray' } |
        Select-Object -First 1
    if ($service) {
        Set-Service -Name $service.Name -StartupType Automatic
        if ($service.Status -ne 'Running') { Start-Service -Name $service.Name }
        Write-Host "Runner service : $($service.Name) [$((Get-Service $service.Name).Status)]" -ForegroundColor Green
    }
    else {
        Write-Warning 'Runner was configured, but its Windows service was not found by wildcard search.'
    }
}

function Remove-ExistingRunnerRegistration {
    if (-not (Test-Path -LiteralPath (Join-Path $RunnerRoot '.runner') -PathType Leaf)) {
        return
    }

    Write-Host 'Existing SC220 runner configuration detected. Re-registering it safely...'
    try {
        $removeToken = Invoke-GhJson api --method POST `
            "repos/$Repository/actions/runners/remove-token"
        Push-Location $RunnerRoot
        try {
            & .\config.cmd remove --unattended --token $removeToken.token
            if ($LASTEXITCODE -ne 0) {
                Write-Warning "Existing runner removal returned $LASTEXITCODE; continuing with replacement."
            }
        }
        finally {
            Pop-Location
        }
    }
    catch {
        Write-Warning "Could not remove the old registration cleanly: $($_.Exception.Message)"
        Get-Service -Name 'actions.runner.*' -ErrorAction SilentlyContinue |
            Where-Object { $_.Name -match 'sc220|masarray' } |
            Stop-Service -Force -ErrorAction SilentlyContinue
    }
}

function New-DesktopReleaseShortcut {
    $releaseCommand = Join-Path (Split-Path -Parent $PSScriptRoot) 'RELEASE-SC220.cmd'
    if (-not (Test-Path -LiteralPath $releaseCommand -PathType Leaf)) { return }

    $desktop = [Environment]::GetFolderPath('Desktop')
    $shortcutPath = Join-Path $desktop 'Release SC220 Live.lnk'
    $shell = New-Object -ComObject WScript.Shell
    $shortcut = $shell.CreateShortcut($shortcutPath)
    $shortcut.TargetPath = $releaseCommand
    $shortcut.WorkingDirectory = Split-Path -Parent $releaseCommand
    $shortcut.Description = 'Build SC220 Live locally and publish it to sc220-download'
    $shortcut.Save()
    Write-Host "Desktop shortcut: $shortcutPath" -ForegroundColor Green
}

if (-not (Test-Administrator)) {
    Write-Host 'Administrator access is required to install build tools and the runner service.'
    Restart-Elevated
    exit $LASTEXITCODE
}

Write-Section 'SC220 Live local runner - one-click setup'
Write-Host 'This setup installs build prerequisites, registers a repository-only runner,'
Write-Host 'and stores release credentials as encrypted GitHub Actions secrets.'
Write-Host 'No GitHub token is written to the repository or a plain-text local file.'

if (-not (Test-Command winget.exe)) {
    throw 'Windows Package Manager (winget) is required. Update App Installer from Microsoft Store, then retry.'
}

Write-Section 'Memeriksa build tools'
if (-not (Test-Command git.exe)) {
    Invoke-WingetInstall -Id 'Git.Git' -MachineScope
}
if (-not (Test-Command gh.exe)) {
    Invoke-WingetInstall -Id 'GitHub.cli' -MachineScope
}
if (-not (Test-Command py.exe) -and -not (Test-Command python.exe)) {
    $pythonId = Resolve-AvailableWingetId @('Python.Python.3.13', 'Python.Python.3.12')
    if (-not $pythonId) { throw 'No supported Python package was found in winget.' }
    Invoke-WingetInstall -Id $pythonId -MachineScope
}
if (-not (Test-Command cmake.exe)) {
    Invoke-WingetInstall -Id 'Kitware.CMake' -MachineScope
}
if (-not (Test-Command ninja.exe)) {
    Invoke-WingetInstall -Id 'Ninja-build.Ninja' -MachineScope
}
if (-not (Test-Path -LiteralPath (Join-Path ${env:ProgramFiles(x86)} 'Inno Setup 6\ISCC.exe') -PathType Leaf -and
    -not (Test-Path -LiteralPath (Join-Path $env:ProgramFiles 'Inno Setup 6\ISCC.exe') -PathType Leaf)) {
    Invoke-WingetInstall -Id 'JRSoftware.InnoSetup' -MachineScope
}

$cpp = Get-CppBuildTools
if (-not $cpp) {
    $buildToolsId = Resolve-AvailableWingetId @(
        'Microsoft.VisualStudio.2026.BuildTools',
        'Microsoft.VisualStudio.2022.BuildTools'
    )
    if (-not $buildToolsId) {
        throw 'Visual Studio Build Tools was not found in winget.'
    }

    $override = '--wait --passive --norestart --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended'
    Invoke-WingetInstall -Id $buildToolsId -Override $override
    $cpp = Get-CppBuildTools
}

Refresh-ProcessPath
foreach ($command in @('git.exe', 'gh.exe', 'cmake.exe')) {
    if (-not (Test-Command $command)) { throw "$command is still unavailable after installation." }
}
if (-not (Test-Command py.exe) -and -not (Test-Command python.exe)) {
    throw 'Python is still unavailable after installation.'
}
if (-not $cpp) {
    throw 'MSVC C++ build tools are still unavailable. Reboot Windows and run SETUP-LOCAL-RUNNER.cmd again.'
}
Write-Host "MSVC toolchain : $($cpp.InstallationPath)" -ForegroundColor Green

Write-Section 'Login GitHub'
& gh.exe auth status -h github.com *> $null
if ($LASTEXITCODE -ne 0) {
    Write-Host 'A browser window will open for GitHub login.'
    & gh.exe auth login --hostname github.com --git-protocol https --web --scopes 'repo,workflow'
    if ($LASTEXITCODE -ne 0) { throw 'GitHub CLI login failed.' }
}

& gh.exe repo view $Repository --json nameWithOwner *> $null
if ($LASTEXITCODE -ne 0) {
    throw "The authenticated GitHub account cannot access $Repository."
}

Write-Section 'Menyiapkan encrypted repository secrets'
$secretNames = Get-RepositorySecretNames

if ($secretNames -notcontains 'ASKP_REPO_TOKEN') {
    Write-Host 'ASKP_REPO_TOKEN is missing.' -ForegroundColor Yellow
    Write-Host 'Paste a fine-grained token with read-only Contents access to masarray/askp-vst.'
    $askpToken = Read-SecretText 'ASK-P read token'
    try { Set-RepositorySecret -Name 'ASKP_REPO_TOKEN' -Value $askpToken }
    finally { $askpToken = $null }
    Write-Host 'ASKP_REPO_TOKEN saved as an encrypted Actions secret.' -ForegroundColor Green
}
else {
    Write-Host 'ASKP_REPO_TOKEN already exists; keeping it unchanged.' -ForegroundColor Green
}

$replaceReleaseToken = $secretNames -notcontains 'SC220_DOWNLOAD_TOKEN'
if (-not $replaceReleaseToken) {
    $answer = Read-Host 'SC220_DOWNLOAD_TOKEN already exists. Replace it? [y/N]'
    $replaceReleaseToken = $answer -match '^(?i)y(?:es)?$'
}
if ($replaceReleaseToken) {
    Write-Host 'Paste a fine-grained token limited to masarray/sc220-download with:'
    Write-Host '  Repository permission -> Contents: Read and write'
    $downloadToken = Read-SecretText 'sc220-download release token'
    try { Set-RepositorySecret -Name 'SC220_DOWNLOAD_TOKEN' -Value $downloadToken }
    finally { $downloadToken = $null }
    Write-Host 'SC220_DOWNLOAD_TOKEN saved as an encrypted Actions secret.' -ForegroundColor Green
}
else {
    Write-Host 'Keeping the existing SC220_DOWNLOAD_TOKEN.' -ForegroundColor Green
}

Write-Section 'Mendaftarkan repository-only self-hosted runner'
Remove-ExistingRunnerRegistration
$registration = Invoke-GhJson api --method POST `
    "repos/$Repository/actions/runners/registration-token"
if (-not $registration.token) {
    throw 'GitHub did not return a runner registration token.'
}
Install-RunnerApplication -RegistrationToken $registration.token

New-DesktopReleaseShortcut

Write-Section 'SELESAI'
Write-Host 'Runner label  : self-hosted, Windows, X64, sc220-build'
Write-Host "Runner folder : $RunnerRoot"
Write-Host 'Release       : double-click RELEASE-SC220.cmd or the desktop shortcut.'
Write-Host 'Billing       : build runs on this PC and does not consume GitHub-hosted runner minutes.'
Write-Host 'Security      : the release workflow is manual-only and never runs for pull requests.'
