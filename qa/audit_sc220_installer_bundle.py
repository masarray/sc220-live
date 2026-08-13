from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

iss_path = ROOT / "installer" / "SC220Live.iss"
probe_path = ROOT / "installer" / "SC220PrerequisiteProbe.ps1"
notice_path = ROOT / "installer" / "VB_AUDIO_NOTICE.txt"
package_path = ROOT / "scripts" / "package-sc220-installer.ps1"
workflow_path = ROOT / ".github" / "workflows" / "installer-release-candidate.yml"
inspect_path = ROOT / ".github" / "workflows" / "inspect-vbcable-pack45.yml"
vc_lock_path = ROOT / "VC_REDIST_AUTHORITY.lock"
vb_lock_path = ROOT / "VB_CABLE_AUTHORITY.lock"

required_paths = (
    iss_path,
    probe_path,
    notice_path,
    package_path,
    workflow_path,
    inspect_path,
    vc_lock_path,
    vb_lock_path,
)

iss = iss_path.read_text(encoding="utf-8")
probe = probe_path.read_text(encoding="utf-8")
notice = notice_path.read_text(encoding="utf-8")
package = package_path.read_text(encoding="utf-8")
workflow = workflow_path.read_text(encoding="utf-8")
inspect = inspect_path.read_text(encoding="utf-8")
vc_lock = vc_lock_path.read_text(encoding="utf-8")
vb_lock = vb_lock_path.read_text(encoding="utf-8")

checks: list[tuple[str, bool]] = []


def check(name: str, condition: bool) -> None:
    checks.append((name, bool(condition)))


check("all smart-installer source files exist", all(path.exists() for path in required_paths))

check(
    "bundle remains restricted to elevated Windows 10/11 x64 installation",
    all(
        token in iss
        for token in (
            "MinVersion=10.0.10240",
            "ArchitecturesAllowed=x64compatible",
            "ArchitecturesInstallIn64BitMode=x64compatible",
            "PrivilegesRequired=admin",
        )
    ),
)

check(
    "Microsoft VC runtime is a required verified installer payload",
    all(
        token in iss
        for token in (
            "VCRedistSource",
            "VC_redist.x64.exe",
            "InstallVCRuntimeIfNeeded",
            "/install /passive /norestart",
            "IsVCRuntimeInstalled",
            "VC\\Runtimes\\x64",
        )
    ),
)

check(
    "runtime installation is skipped when the x64 registry state is present",
    "if IsVCRuntimeInstalled then" in iss
    and "skipping redistributable setup" in iss
    and "Installed', Installed" in iss,
)

check(
    "runtime failures prevent automatic application launch",
    all(
        token in iss
        for token in (
            "CorePrerequisitesReady",
            "automatic launch has been disabled",
            "function CanLaunchApp",
            "Result := CorePrerequisitesReady",
        )
    ),
)

check(
    "PowerShell prerequisite probe is retained for installation and diagnostics",
    all(
        token in iss
        for token in (
            "SC220PrerequisiteProbe.ps1",
            'DestDir: "{app}\\Tools"',
            "Flags: dontcopy",
            "RunPrerequisiteProbe('Core')",
            "RunPrerequisiteProbe('VB-Cable')",
        )
    ),
)

check(
    "probe checks VC runtime and both Windows Audio services",
    all(
        token in probe
        for token in (
            "VC\\Runtimes\\x64",
            "Get-Service -Name",
            "Audiosrv",
            "AudioEndpointBuilder",
            "exit 11",
            "exit 12",
        )
    ),
)

check(
    "probe checks VB-CABLE driver and both endpoint identities",
    all(
        token in probe
        for token in (
            "Get-PnpDevice -PresentOnly",
            "CABLE Input",
            "CABLE Output",
            "driverFound",
            "cableInputFound",
            "cableOutputFound",
            "exit 21",
            "exit 22",
        )
    ),
)

check(
    "installer automatically deselects healthy VB-CABLE and selects missing or partial installations",
    "InitialVBCableProbeCode = 0" in iss
    and "WizardSelectTasks('!vbcable')" in iss
    and "WizardSelectTasks('vbcable')" in iss,
)

check(
    "standard VB-CABLE remains an optional visible donationware task",
    all(
        token in iss
        for token in (
            'Name: "vbcable"',
            "VB-Audio VB-CABLE (donationware)",
            "WizardIsTaskSelected('vbcable')",
            "VB-Audio information, donation, and licensing",
            "https://vb-audio.com/Cable/",
        )
    ),
)

check(
    "complete reviewed VB-CABLE package remains staged beside support files",
    all(
        token in iss
        for token in (
            'Source: "{#VBPackageSource}\\*"',
            'DestDir: "{#VBTempDir}"',
            "recursesubdirs",
            "createallsubdirs",
            "deleteafterinstall",
            "Tasks: vbcable",
        )
    ),
)

check(
    "driver install preserves visible Windows consent and restart behavior",
    all(
        token in iss
        for token in (
            "Exec(SetupPath, '-i -h'",
            "SW_SHOWNORMAL",
            "ewWaitUntilTerminated",
            "InstallerRestartRequired := True",
            "function NeedRestart",
        )
    ),
)

check(
    "packager requires the immutable Microsoft runtime authority lock",
    all(
        token in package
        for token in (
            "VC_REDIST_AUTHORITY.lock",
            "download.visualstudio.microsoft.com",
            "VC_REDIST_X64_SHA256",
            "package_sha256",
            "setup_signer_contains",
        )
    ),
)

check(
    "Microsoft runtime is hash-pinned before Authenticode and signer validation",
    all(
        token in package
        for token in (
            "Get-FileHash -LiteralPath $downloadedVCRedist -Algorithm SHA256",
            "$actualVCRedistHash -ne $expectedVCRedistHash",
            "Get-AuthenticodeSignature -LiteralPath $downloadedVCRedist",
            "SignatureStatus]::Valid",
            "Unexpected VC Redistributable signer",
        )
    )
    and package.find("$actualVCRedistHash -ne $expectedVCRedistHash")
    < package.find("Get-AuthenticodeSignature -LiteralPath $downloadedVCRedist"),
)

check(
    "packager passes runtime and probe sources into Inno Setup",
    all(
        token in package
        for token in (
            "Write-IsppDefine $defines 'VCRedistSource'",
            "Write-IsppDefine $defines 'PrerequisiteProbeSource'",
            "SC220PrerequisiteProbe.ps1",
        )
    ),
)

check(
    "VC authority lock pins official source hash signature and registry contract",
    all(
        token in vc_lock
        for token in (
            "authority=Microsoft Visual C++ Redistributable v14 x64",
            "permalink=https://aka.ms/vc14/vc_redist.x64.exe",
            "source=https://download.visualstudio.microsoft.com/",
            "package_sha256=",
            "setup_signature=Valid",
            "setup_signer_contains=Microsoft Corporation",
            "registry_key=SOFTWARE\\Microsoft\\VisualStudio\\14.0\\VC\\Runtimes\\x64",
        )
    ),
)

check(
    "VB-CABLE third-party archive remains pinned before extraction",
    all(
        token in package
        for token in (
            "VBCABLE_PACK45_SHA256",
            "Get-FileHash -LiteralPath $downloadedVBCableZip -Algorithm SHA256",
            "$actualVBCableHash -ne $expectedVBCableHash",
            "supply-chain verification failed",
            "Expand-Archive",
        )
    )
    and package.find("$actualVBCableHash -ne $expectedVBCableHash")
    < package.find("Expand-Archive"),
)

check(
    "VB-CABLE setup INF SYS and CAT remain required from one package root",
    all(
        token in package
        for token in (
            "$vbPackageRoot = $vbSetup.Directory.FullName",
            "$vbPackageFiles = @(Get-ChildItem -LiteralPath $vbPackageRoot -Recurse -File)",
            "foreach ($extension in @('.inf', '.sys', '.cat'))",
            "missing required driver support files",
            "Write-IsppDefine $defines 'VBPackageSource'",
        )
    ),
)

check(
    "VB-CABLE executable and catalogue signatures remain validated",
    all(
        token in package
        for token in (
            "Get-AuthenticodeSignature -LiteralPath $vbSetup.FullName",
            "Get-ChildItem -LiteralPath $vbPackageRoot -Recurse -File -Filter '*.cat'",
            "catalogue signature is invalid",
        )
    ),
)

check(
    "additional A B C D cable packages remain rejected",
    "forbiddenAdditionalCable" in package
    and "standard bundle unexpectedly contains A/B/C/D cable material" in package,
)

check(
    "manifest records smart prerequisite runtime and VB-CABLE supply-chain data",
    all(
        token in package
        for token in (
            "smart_prerequisites",
            "vc_runtime",
            "vc_runtime_registry",
            "audio_services",
            "vb_cable_endpoints",
            "installer_sha256",
            "source_commit",
        )
    ),
)

check(
    "archived installer workflow retains its contract while remaining inert",
    all(
        token in workflow
        for token in (
            "on: []",
            "audit_sc220_installer_bundle.py",
            "package-sc220-installer.ps1",
            "innosetup --version=6.7.1",
            "actions/upload-artifact@v4",
            "Setup-win-x64.exe",
            "Setup-win-x64.manifest.json",
        )
    ),
)

check(
    "archived Pack45 inspection remains inert but preserves review logic",
    all(
        token in inspect
        for token in (
            "on: []",
            "VBCABLE_Driver_Pack45.zip",
            "Get-FileHash -LiteralPath $zip -Algorithm SHA256",
            "Get-AuthenticodeSignature -LiteralPath $setup.FullName",
            "Invalid catalogue signature",
            "A/B/C/D material",
        )
    ),
)

check(
    "authority and donationware notices preserve product separation",
    "additional_cable_material=false" in vb_lock
    and all(
        token in notice
        for token in (
            "VB-Audio Software",
            "donationware",
            "https://vb-audio.com/Cable/",
            "requires a Windows restart",
            "not VB-CABLE A+B or C+D",
            "separate products",
        )
    ),
)

failed = [name for name, ok in checks if not ok]
print("SC220 Live smart prerequisite installer audit")
for name, ok in checks:
    print(f"[ {'PASS' if ok else 'FAIL'} ] {name}")
print(f"Checks: {len(checks)}  Passed: {len(checks) - len(failed)}  Failed: {len(failed)}")
raise SystemExit(1 if failed else 0)
