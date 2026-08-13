from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

cmake = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
readme = (ROOT / "README.md").read_text(encoding="utf-8")
portable = (ROOT / "scripts" / "package-sc220-windows.ps1").read_text(encoding="utf-8")
installer = (ROOT / "scripts" / "package-sc220-installer.ps1").read_text(encoding="utf-8")
local_release = (ROOT / "scripts" / "start-sc220-local-release.ps1").read_text(encoding="utf-8")
handoff = (ROOT / "scripts" / "publish-sc220-download.ps1").read_text(encoding="utf-8")
approve = (ROOT / "scripts" / "approve-sc220-public-release.ps1").read_text(encoding="utf-8")
landing = (ROOT / "release-site" / "index.html").read_text(encoding="utf-8")
latest = (ROOT / "release-site" / "latest.json").read_text(encoding="utf-8")
private_workflows = "\n".join(
    path.read_text(encoding="utf-8")
    for path in sorted((ROOT / ".github" / "workflows").glob("*.yml"))
)
gate = (ROOT / "docs" / "PUBLIC_RELEASE_GATE.md").read_text(encoding="utf-8")
notes = (ROOT / "docs" / "RELEASE_NOTES_v0.1.0.md").read_text(encoding="utf-8")
third_party = (ROOT / "THIRD_PARTY_NOTICES.md").read_text(encoding="utf-8")

checks: list[tuple[str, bool]] = []


def check(name: str, condition: bool) -> None:
    checks.append((name, bool(condition)))


check(
    "current source and public receipt agree on v0.1.1",
    "project(SC220Live VERSION 0.1.1" in cmake
    and "SC220 Live v0.1.1" in readme
    and "Current published receipt" in gate
    and "v0.1.1" in gate,
)
check(
    "README describes current selectable source and Quick Stream architecture",
    all(token in readme for token in (
        "selectable Windows playback endpoint",
        "Physical Monitor Bus",
        "Quick Stream routing",
        "standard VB-Audio VB-CABLE",
        "CABLE Input",
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
    "local release accepts only a clean synchronized private main commit",
    all(token in local_release for token in (
        "$branch -ne 'main'",
        "status --porcelain",
        "fetch origin main",
        "$sourceCommit = (& git.exe rev-parse HEAD).Trim()",
        "$remoteCommit = (& git.exe rev-parse origin/main).Trim()",
        "$sourceCommit -ne $remoteCommit",
    )),
)
check(
    "private repository contains no GitHub workflow definitions",
    not list((ROOT / ".github" / "workflows").glob("*.yml"))
    and not private_workflows,
)
check(
    "local release builds audits and packages before public handoff",
    all(token in local_release for token in (
        "run-sc220-local-qa.ps1",
        "package-sc220-installer.ps1",
        "publish-sc220-download.ps1",
        "QA/build/package: local Windows workstation",
        "binary-only draft -> public sc220-download workflow",
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
    "binary handoff uploads the exact audited public asset set as a draft",
    all(token in handoff for token in (
        "$installerName",
        "$checksumName",
        "$manifestName",
        "BUILD_COMMIT.txt",
        "VC_REDIST_X64_SHA256.txt",
        "VB_CABLE_PACK45_SHA256.txt",
        "--draft",
    )),
)
check(
    "only the public repository workflow can publish the draft",
    "publish-draft-release.yml" in handoff
    and "--repo $TargetRepository" in handoff
    and "Invoke-Gh run watch" in handoff
    and "release edit" not in handoff
    and "git push" not in handoff,
)
check(
    "release gate requires installer physical listening presentation and screenshot checks",
    all(token in gate for token in (
        "Windows x64 Release executable",
        "Mandatory Quick Stream listening test",
        "Installer qualification",
        "extended playback",
        "screenshot and UI copy",
        "VB-CABLE Pack45 SHA-256",
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
