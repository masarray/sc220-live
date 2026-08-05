from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

cmake = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
readme = (ROOT / "README.md").read_text(encoding="utf-8")
package = (ROOT / "scripts" / "package-sc220-windows.ps1").read_text(encoding="utf-8")
release = (ROOT / "scripts" / "release-sc220-windows.ps1").read_text(encoding="utf-8")
approve = (ROOT / "scripts" / "approve-sc220-public-release.ps1").read_text(encoding="utf-8")
landing = (ROOT / "release-site" / "index.html").read_text(encoding="utf-8")
workflow = (ROOT / ".github" / "workflows" / "release-candidate.yml").read_text(encoding="utf-8")
gate = (ROOT / "docs" / "PUBLIC_RELEASE_GATE.md").read_text(encoding="utf-8")
notes = (ROOT / "docs" / "RELEASE_NOTES_v0.1.0.md").read_text(encoding="utf-8")

checks: list[tuple[str, bool]] = []


def check(name: str, condition: bool) -> None:
    checks.append((name, bool(condition)))


check(
    "initial public version and release notes agree",
    "project(SC220Live VERSION 0.1.0" in cmake
    and "# SC220 Live v0.1.0" in notes,
)
check(
    "README describes current selectable source and physical monitor architecture",
    all(token in readme for token in (
        "selectable Windows playback endpoint",
        "OBS-style WASAPI shared-mode Monitor Bus",
        "PROGRAM` and pre-ducking `MUSIC CUE",
        "VB-CABLE family A",
        "VB-CABLE family B",
    )),
)
check(
    "README no longer claims default-only capture or disabled monitoring",
    all(token not in readme for token in (
        "default Windows playback mix",
        "monitor control is deliberately disabled",
        "default Windows PC Audio loopback",
    )),
)
check(
    "portable quick start teaches distinct source stream and monitor routing",
    all(token in package for token in (
        "VB-CABLE family A",
        "VB-CABLE family B",
        "MONITOR OUTPUT",
        "PROGRAM",
        "MUSIC CUE",
        "IMPORTANT ANTI-ECHO RULE",
    )),
)
check(
    "portable quick start does not publish obsolete default-mix guidance",
    "captures the default Windows playback mix" not in package,
)
check(
    "public landing page explains current stream and monitor workflow",
    all(token in landing for token in (
        "STREAM + MONITOR",
        "Cable A",
        "Cable B",
        "Monitor Output",
        "PROGRAM",
        "MUSIC CUE",
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
    "manual release candidate workflow builds full private authority package",
    all(token in workflow for token in (
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
    "release gate requires binary listening presentation and screenshot checks",
    all(token in gate for token in (
        "Full `SC220 Live.exe` clean Release build",
        "Mandatory Windows listening test",
        "Ten-minute playback",
        "SC220_LIVE_UX_LOCKED.png",
        "Do not merge",
    )),
)
check(
    "release notes state anti-echo routing and current core features",
    all(token in notes for token in (
        "Independent Voice Signature and Music Signature",
        "OBS-style native WASAPI Monitor Bus",
        "14-band rhythmic programme analyzer",
        "VB-CABLE family A",
        "VB-CABLE family B",
    )),
)
check(
    "required public release assets exist",
    all(path.exists() for path in (
        ROOT / "docs" / "SC220_LIVE_UX_LOCKED.png",
        ROOT / "release-site" / "index.html",
        ROOT / "release-site" / "styles.css",
        ROOT / "release-site" / "latest.json",
        ROOT / "THIRD_PARTY_NOTICES.md",
        ROOT / "EULA.txt",
        ROOT / "PURCHASE_TERMS.txt",
    )),
)

failed = [name for name, ok in checks if not ok]
print("SC220 Live public release readiness contract audit")
for name, ok in checks:
    print(f"[ {'PASS' if ok else 'FAIL'} ] {name}")
print(f"Checks: {len(checks)}  Passed: {len(checks) - len(failed)}  Failed: {len(failed)}")
raise SystemExit(1 if failed else 0)
