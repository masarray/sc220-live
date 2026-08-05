from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

cmake = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
readme = (ROOT / "README.md").read_text(encoding="utf-8")
portable = (ROOT / "scripts" / "package-sc220-windows.ps1").read_text(encoding="utf-8")
installer = (ROOT / "scripts" / "package-sc220-installer.ps1").read_text(encoding="utf-8")
release = (ROOT / "scripts" / "release-sc220-windows.ps1").read_text(encoding="utf-8")
approve = (ROOT / "scripts" / "approve-sc220-public-release.ps1").read_text(encoding="utf-8")
landing = (ROOT / "release-site" / "index.html").read_text(encoding="utf-8")
latest = (ROOT / "release-site" / "latest.json").read_text(encoding="utf-8")
portable_workflow = (ROOT / ".github" / "workflows" / "release-candidate.yml").read_text(encoding="utf-8")
installer_workflow = (ROOT / ".github" / "workflows" / "installer-bundle.yml").read_text(encoding="utf-8")
gate = (ROOT / "docs" / "PUBLIC_RELEASE_GATE.md").read_text(encoding="utf-8")
notes = (ROOT / "docs" / "RELEASE_NOTES_v0.1.0.md").read_text(encoding="utf-8")
third_party = (ROOT / "THIRD_PARTY_NOTICES.md").read_text(encoding="utf-8")

checks: list[tuple[str, bool]] = []


def check(name: str, condition: bool) -> None:
    checks.append((name, bool(condition)))


check(
    "initial public version and release notes agree",
    "project(SC220Live VERSION 0.1.0" in cmake
    and "# SC220 Live v0.1.0" in notes,
)
check(
    "README describes current selectable source and Quick Stream architecture",
    all(token in readme for token in (
        "selectable Windows playback endpoint",
        "OBS-style WASAPI shared-mode Monitor Bus",
        "Quick Stream routing",
        "standard VB-Audio VB-CABLE",
        "CABLE Input (VB-Audio Virtual Cable)",
        "CABLE Output",
    )),
)
check(
    "README no longer publishes obsolete default-only or A/B bundle guidance",
    all(token not in readme for token in (
        "default Windows playback mix",
        "monitor control is deliberately disabled",
        "VB-CABLE family A",
        "VB-CABLE family B",
    )),
)
check(
    "portable quick start teaches standard VB-CABLE Quick Stream routing",
    all(token in portable for token in (
        "QUICK STREAM ROUTING",
        "CABLE Input (VB-Audio Virtual Cable)",
        "CABLE Output",
        "normal Windows playback endpoint",
        "IMPORTANT ANTI-ECHO RULE",
        "This portable build does not install audio drivers",
    )),
)
check(
    "portable quick start does not publish obsolete multi-cable requirement",
    all(token not in portable for token in (
        "VB-CABLE family A",
        "VB-CABLE family B",
        "Cable A",
        "Cable B",
    )),
)
check(
    "public landing page promotes verified installer and safe Quick Stream workflow",
    all(token in landing for token in (
        "Unduh Installer",
        "Versi Portable",
        "VB-CABLE → OBS",
        "CABLE Input",
        "CABLE Output",
        "Supply-chain manifest",
        "donationware",
    )),
)
check(
    "latest metadata exposes installer portable and VB-CABLE identity",
    all(token in latest for token in (
        '"installer"',
        '"portable"',
        '"vb_cable"',
        '"VBCABLE_Driver_Pack45.zip"',
        '"donationware": true',
        '"manifest": "{{MANIFEST_URL}}"',
    )),
)
check(
    "release script only accepts a clean canonical private main commit",
    all(token in release for token in (
        "Public release must run from main",
        "Working tree has unstaged changes",
        "Working tree has staged changes",
        "Fetching canonical private main branch",
        "$sourceCommit = (& git rev-parse HEAD).Trim()",
        "$remoteCommit = (& git rev-parse origin/main).Trim()",
        "if ($sourceCommit -ne $remoteCommit)",
        "HEAD must exactly match origin/main",
    ))
    and "Pushing private source authority" not in release
    and "Committing private source" not in release,
)
check(
    "public HEAD-to-main push is confined to isolated download checkout",
    all(token in release for token in (
        "Push-Location $publicCheckout",
        "Pushing public landing page",
        "git push -u origin HEAD:main",
        "Pop-Location",
    ))
    and release.find("Push-Location $publicCheckout")
        < release.find("git push -u origin HEAD:main")
        < release.find("Pop-Location"),
)
check(
    "release script requires exact recent validation receipt",
    all(token in release for token in (
        "public-release-validation.json",
        "$receipt.version -ne $Version",
        "$receipt.commit -ne $sourceCommit",
        "TotalHours -gt 72",
        "approve-sc220-public-release.ps1",
    )),
)
check(
    "release approval binds user attestation to exact main commit",
    all(token in approve for token in (
        "$currentBranch -ne 'main'",
        "$head -ne $remote",
        '"RELEASE $Version $short"',
        "docs\\PUBLIC_RELEASE_GATE.md",
        "public-release-validation.json",
    )),
)
check(
    "manual portable release candidate builds full private authority package",
    all(token in portable_workflow for token in (
        "workflow_dispatch",
        "ASKP_REPO_TOKEN",
        "repository: masarray/askp-vst",
        "5757119cab7a1a75f09dcf0edea6e3be97975439",
        "package-sc220-windows.ps1",
        "actions/upload-artifact@v4",
        "BUILD_COMMIT.txt",
    )),
)
check(
    "manual installer workflow pins private ASK-P and official VB-CABLE authority",
    all(token in installer_workflow for token in (
        "workflow_dispatch",
        "ASKP_REPO_TOKEN",
        "vars.VBCABLE_PACK45_SHA256",
        "repository: masarray/askp-vst",
        "5757119cab7a1a75f09dcf0edea6e3be97975439",
        "package-sc220-installer.ps1",
        "actions/upload-artifact@v4",
    )),
)
check(
    "public release builds and publishes installer checksum manifest and portable",
    all(token in release for token in (
        "package-sc220-installer.ps1",
        "VBCABLE_PACK45_SHA256",
        "$installerPackage.Installer",
        "$installerPackage.HashFile",
        "$installerPackage.Manifest",
        "$portablePackage.Zip",
        "$portablePackage.HashFile",
        "VB-CABLE Pack45 SHA-256",
    )),
)
check(
    "release gate requires installer physical listening presentation and screenshot checks",
    all(token in gate for token in (
        "Full `SC220 Live.exe` clean Release build",
        "Mandatory Quick Stream listening test",
        "Installer qualification",
        "Ten-minute playback",
        "SC220_LIVE_UX_LOCKED.png",
        "VBCABLE_PACK45_SHA256",
    )),
)
check(
    "release notes state current core features and standard VB-CABLE bundle",
    all(token in notes for token in (
        "Independent Voice Signature and Music Signature",
        "OBS-style native WASAPI Monitor Bus",
        "14-band rhythmic programme analyzer",
        "standard VB-Audio VB-CABLE",
        "CABLE Input",
        "CABLE Output",
        "does not contain VB-CABLE A+B or C+D",
    )),
)
check(
    "third-party notice preserves VB-Audio ownership donationware and separation",
    all(token in third_party for token in (
        "VB-Audio VB-CABLE standard",
        "donationware",
        "https://vb-audio.com/Cable/",
        "separate product",
        "does not include",
    )),
)
check(
    "required public release assets and installer sources exist",
    all(path.exists() for path in (
        ROOT / "docs" / "SC220_LIVE_UX_LOCKED.png",
        ROOT / "release-site" / "index.html",
        ROOT / "release-site" / "styles.css",
        ROOT / "release-site" / "latest.json",
        ROOT / "installer" / "SC220Live.iss",
        ROOT / "installer" / "VB_AUDIO_NOTICE.txt",
        ROOT / "scripts" / "package-sc220-installer.ps1",
        ROOT / "THIRD_PARTY_NOTICES.md",
        ROOT / "EULA.txt",
        ROOT / "PURCHASE_TERMS.txt",
    )),
)
check(
    "installer packager remains the official verified standard-cable path",
    all(token in installer for token in (
        "VBCABLE_Driver_Pack45.zip",
        "VBCABLE_Setup_x64.exe",
        "Get-AuthenticodeSignature",
        "VBCABLE_PACK45_SHA256",
    )),
)

failed = [name for name, ok in checks if not ok]
print("SC220 Live public release readiness contract audit")
for name, ok in checks:
    print(f"[ {'PASS' if ok else 'FAIL'} ] {name}")
print(f"Checks: {len(checks)}  Passed: {len(checks) - len(failed)}  Failed: {len(failed)}")
raise SystemExit(1 if failed else 0)
