from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

iss_path = ROOT / "installer" / "SC220Live.iss"
notice_path = ROOT / "installer" / "VB_AUDIO_NOTICE.txt"
package_path = ROOT / "scripts" / "package-sc220-installer.ps1"
workflow_path = ROOT / ".github" / "workflows" / "installer-bundle.yml"

iss = iss_path.read_text(encoding="utf-8")
notice = notice_path.read_text(encoding="utf-8")
package = package_path.read_text(encoding="utf-8")
workflow = workflow_path.read_text(encoding="utf-8")

checks: list[tuple[str, bool]] = []


def check(name: str, condition: bool) -> None:
    checks.append((name, bool(condition)))


check(
    "installer source files exist",
    all(path.exists() for path in (iss_path, notice_path, package_path, workflow_path)),
)
check(
    "bundle is restricted to Windows 10 or newer x64",
    all(token in iss for token in (
        "MinVersion=10.0.10240",
        "ArchitecturesAllowed=x64compatible",
        "ArchitecturesInstallIn64BitMode=x64compatible",
        "PrivilegesRequired=admin",
    )),
)
check(
    "standard VB-CABLE remains an optional visible donationware task",
    all(token in iss for token in (
        'Name: "vbcable"',
        "VB-Audio VB-CABLE (donationware)",
        "WizardIsTaskSelected('vbcable')",
        "VB-Audio information, donation, and licensing",
        "https://vb-audio.com/Cable/",
    )),
)
check(
    "driver install keeps Windows consent and restart semantics",
    all(token in iss for token in (
        "Exec(SetupPath, '-i -h'",
        "SW_SHOWNORMAL",
        "ewWaitUntilTerminated",
        "VBCableInstallSucceeded := True",
        "function NeedRestart",
    )),
)
check(
    "SC220 app is not launched before a required VB-CABLE restart",
    "function CanLaunchApp" in iss
    and "Result := not VBCableInstallSucceeded" in iss
    and "Check: CanLaunchApp" in iss,
)
check(
    "installer includes required app and legal payload",
    all(token in iss for token in (
        "SC220 Live.exe",
        "EULA.txt",
        "PURCHASE_TERMS.txt",
        "THIRD_PARTY_NOTICES.md",
        "VB_AUDIO_NOTICE.txt",
    )),
)
check(
    "packager downloads only the official standard Pack45 package",
    "https://download.vb-audio.com/Download_CABLE/VBCABLE_Driver_Pack45.zip" in package
    and "VBCABLE_Setup_x64.exe" in package
    and "VBCABLE_A_B" not in package
    and "VBCable_AB" not in package
    and "VBCable_CD" not in package,
)
check(
    "third-party archive is pinned before extraction and compilation",
    all(token in package for token in (
        "VBCABLE_PACK45_SHA256",
        "Get-FileHash -LiteralPath $downloadedZip -Algorithm SHA256",
        "$actualVBCableHash -ne $expectedVBCableHash",
        "supply-chain verification failed",
    ))
    and package.find("$actualVBCableHash -ne $expectedVBCableHash")
        < package.find("Expand-Archive"),
)
check(
    "driver executable and catalog signatures are validated",
    all(token in package for token in (
        "Get-AuthenticodeSignature -LiteralPath $vbSetup.FullName",
        "SignatureStatus]::Valid",
        "Get-ChildItem -LiteralPath $expanded -Recurse -File -Filter '*.cat'",
        "catalogue signature is invalid",
    )),
)
check(
    "additional A B C D cable packages are rejected",
    "forbiddenAdditionalCable" in package
    and "standard bundle unexpectedly contains A/B/C/D cable material" in package,
)
check(
    "installer output has checksum and supply-chain manifest",
    all(token in package for token in (
        '$hashPath = "$installer.sha256"',
        "Setup-win-x64.manifest.json",
        "installer_sha256",
        "source_commit",
        "signer",
        "authenticode",
    )),
)
check(
    "workflow requires private ASK-P and reviewed VB-CABLE hash",
    all(token in workflow for token in (
        "ASKP_REPO_TOKEN",
        "vars.VBCABLE_PACK45_SHA256",
        "repository: masarray/askp-vst",
        "5757119cab7a1a75f09dcf0edea6e3be97975439",
        "Require pinned private and third-party authorities",
    )),
)
check(
    "workflow uses pinned Inno Setup and app-only smoke install",
    all(token in workflow for token in (
        "innosetup --version=6.7.1",
        "package-sc220-installer.ps1",
        "Smoke install application payload without driver changes",
        '/TASKS=\"!vbcable,!desktopicon\"',
        "unins000.exe",
    )),
)
check(
    "workflow publishes installer checksum and manifest only as artifacts",
    all(token in workflow for token in (
        "actions/upload-artifact@v4",
        "Setup-win-x64.exe",
        "Setup-win-x64.exe.sha256",
        "Setup-win-x64.manifest.json",
    )),
)
check(
    "donationware notice preserves origin choice restart and product separation",
    all(token in notice for token in (
        "VB-Audio Software",
        "donationware",
        "https://vb-audio.com/Cable/",
        "https://shop.vb-audio.com/en/win-apps/11-vb-cable.html",
        "requires a Windows restart",
        "not VB-CABLE A+B or C+D",
        "separate products",
    )),
)

failed = [name for name, ok in checks if not ok]
print("SC220 Live verified VB-CABLE installer bundle audit")
for name, ok in checks:
    print(f"[ {'PASS' if ok else 'FAIL'} ] {name}")
print(f"Checks: {len(checks)}  Passed: {len(checks) - len(failed)}  Failed: {len(failed)}")
raise SystemExit(1 if failed else 0)
