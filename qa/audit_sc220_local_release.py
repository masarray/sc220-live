from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
WORKFLOWS = sorted((ROOT / ".github/workflows").glob("*.yml"))

local_qa = (ROOT / "scripts/run-sc220-local-qa.ps1").read_text(encoding="utf-8")
start = (ROOT / "scripts/start-sc220-local-release.ps1").read_text(encoding="utf-8")
publish = (ROOT / "scripts/publish-sc220-download.ps1").read_text(encoding="utf-8")
release_cmd = (ROOT / "RELEASE-SC220.cmd").read_text(encoding="utf-8")

checks: list[tuple[str, bool]] = []


def check(name: str, condition: bool) -> None:
    checks.append((name, bool(condition)))


check("all private workflows are inert", bool(WORKFLOWS) and all(
    re.search(r"(?m)^on: \[\]$", path.read_text(encoding="utf-8"))
    for path in WORKFLOWS
))
check("local QA owns source generation and Release build", all(token in local_qa for token in (
    "build-live-windows.ps1",
    "Configuration = 'Release'",
    "audit_sc220_premium_alignment.py",
    "audit_v0150_askp_p03_port.py",
    "verify_fixed_fft.py",
    "ctest --test-dir",
    "git diff --check",
)))
check("local QA protects the tracked FFT reference", all(token in local_qa for token in (
    "ReadAllBytes($fftReference)",
    "WriteAllBytes($fftReference, $fftBytes)",
)))
check("release launcher never dispatches a private workflow", all(token not in start for token in (
    "local-release.yml",
    "Start-Service",
    "actions-runner",
    "gh.exe workflow run",
)) and all(token in start for token in (
    "run-sc220-local-qa.ps1",
    "package-sc220-installer.ps1",
    "publish-sc220-download.ps1",
)))
check("release source must be clean synchronized private main", all(token in start for token in (
    "git.exe status --porcelain",
    "git.exe fetch origin main",
    "Release source must equal origin/main",
)))
check("publisher targets only the public download repository", all(token in publish for token in (
    "[string]$TargetRepository = 'masarray/sc220-download'",
    "SC220-Live-v$Version-Setup-win-x64.exe",
    "BUILD_COMMIT.txt",
    "VC_REDIST_X64_SHA256.txt",
    "VB_CABLE_PACK45_SHA256.txt",
)))
check("publisher uploads an unpublished binary-only draft", all(token in publish for token in (
    "'--draft'",
    "Binary handoff $tag is not an unpublished draft",
    "Draft release is missing assets",
)) and "Set-RepositoryTextFile" not in publish)
check("public sc220-download workflow is the publication authority", all(token in publish for token in (
    "workflow run 'publish-draft-release.yml'",
    "--repo $TargetRepository",
    "Invoke-Gh run watch",
    "repos/$TargetRepository/releases/latest",
    "Public workflow completed without publishing and promoting $tag",
)))
check("launcher verifies public repository visibility", all(token in start for token in (
    "gh.exe repo view masarray/sc220-download",
    "masarray/sc220-download must be reachable and public",
)))
check("release wrapper invokes only the local launcher",
      "start-sc220-local-release.ps1" in release_cmd
      and "local-release.yml" not in release_cmd
      and "gh workflow" not in release_cmd.lower())

combined = "\n".join(path.read_text(encoding="utf-8") for path in WORKFLOWS)
combined += "\n" + local_qa + start + publish + release_cmd
check("release path contains no obvious literal credential", not re.search(
    r"(?:ghp_[A-Za-z0-9]{20,}|github_pat_[A-Za-z0-9_]{20,}|sk-[A-Za-z0-9]{20,})",
    combined,
))

failed = [name for name, ok in checks if not ok]
print("SC220 local QA and public draft release audit")
for name, ok in checks:
    print(f"[ {'PASS' if ok else 'FAIL'} ] {name}")
print(f"Checks: {len(checks)}  Passed: {len(checks) - len(failed)}  Failed: {len(failed)}")
raise SystemExit(1 if failed else 0)
