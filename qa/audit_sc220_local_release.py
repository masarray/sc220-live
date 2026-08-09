from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]

paths = {
    "workflow": ROOT / ".github" / "workflows" / "local-release.yml",
    "setup": ROOT / "scripts" / "setup-sc220-local-runner.ps1",
    "configure": ROOT / "scripts" / "configure-sc220-runner-service.ps1",
    "verify": ROOT / "scripts" / "verify-sc220-runner-host.ps1",
    "start": ROOT / "scripts" / "start-sc220-local-release.ps1",
    "publish": ROOT / "scripts" / "publish-sc220-download.ps1",
    "setup_cmd": ROOT / "SETUP-LOCAL-RUNNER.cmd",
    "release_cmd": ROOT / "RELEASE-SC220.cmd",
}

text = {name: path.read_text(encoding="utf-8") for name, path in paths.items()}
workflow = text["workflow"]
setup = text["setup"]
configure = text["configure"]
verify = text["verify"]
start = text["start"]
publish = text["publish"]
setup_cmd = text["setup_cmd"]
release_cmd = text["release_cmd"]

checks: list[tuple[str, bool]] = []


def check(name: str, condition: bool) -> None:
    checks.append((name, bool(condition)))


approved_askp_commit = "2c764f82b0fc99eaff04d918cef4f839db7509e5"

check("all one-click release files exist", all(path.exists() for path in paths.values()))
check(
    "release workflow is manual-only",
    "workflow_dispatch:" in workflow
    and not re.search(r"(?m)^\s{2}(?:push|pull_request|pull_request_target|schedule):", workflow),
)
check(
    "workflow uses only the SC220 self-hosted Windows label",
    "runs-on: [self-hosted, Windows, X64, sc220-build]" in workflow,
)
check(
    "workflow uses stock Windows PowerShell",
    "shell: pwsh" not in workflow and workflow.count("shell: powershell") >= 6,
)
check(
    "workflow serializes public releases",
    "group: sc220-live-local-public-release" in workflow
    and "cancel-in-progress: false" in workflow,
)
check(
    "checkouts do not persist credentials",
    workflow.count("persist-credentials: false") >= 2,
)
check(
    "ASK-P authority remains pinned and lock-verified",
    "repository: masarray/askp-vst" in workflow
    and approved_askp_commit in workflow
    and "ASKP_AUTHORITY.lock" in workflow
    and "ASK-P checkout mismatch" in workflow,
)
check(
    "release workflow reruns ASK-P port audit before build",
    "qa\\audit_v0150_askp_p03_port.py" in workflow,
)
check(
    "public release token is exposed only to the publish step",
    workflow.count("SC220_DOWNLOAD_TOKEN") == 1
    and "GH_TOKEN: ${{ secrets.SC220_DOWNLOAD_TOKEN }}" in workflow
    and not re.search(r"(?m)^\s{4}GH_TOKEN:", workflow),
)
check(
    "workflow stores no Actions artifact",
    "actions/upload-artifact" not in workflow
    and "actions/download-artifact" not in workflow,
)
check(
    "smoke install never changes VB-CABLE",
    '/TASKS="!vbcable,!desktopicon"' in workflow
    and "SC220PrerequisiteProbe.ps1" in workflow
    and "unins000.exe" in workflow,
)
check(
    "publication follows audit build and smoke test",
    all(
        marker in workflow
        for marker in (
            "Validate source and installer contracts",
            "Build verified smart installer locally",
            "Smoke install, prerequisite probe, and uninstall",
            "Publish release and update sc220-download website",
        )
    )
    and workflow.find("Validate source and installer contracts")
    < workflow.find("Build verified smart installer locally")
    < workflow.find("Smoke install, prerequisite probe, and uninstall")
    < workflow.find("Publish release and update sc220-download website"),
)
check(
    "setup downloads official repository runner and registers a service",
    "https://api.github.com/repos/actions/runner/releases/latest" in setup
    and "actions-runner-win-x64" in setup
    and "--runasservice" in setup
    and "--labels $RunnerLabel" in setup
    and "actions/runners/registration-token" in setup,
)
check(
    "runner is repository-scoped and custom-labelled",
    "--url \"https://github.com/$Repository\"" in setup
    and "[string]$RunnerLabel = 'sc220-build'" in setup
    and r"C:\actions-runner-sc220" in setup,
)
check(
    "runner package digest is verified when available",
    "asset.digest" in setup
    and "Get-FileHash -LiteralPath $archive -Algorithm SHA256" in setup
    and "GitHub runner SHA-256 mismatch" in setup,
)
check(
    "setup stores credentials only as encrypted Actions secrets",
    "gh.exe secret set" in setup
    and "ASKP_REPO_TOKEN" in setup
    and "SC220_DOWNLOAD_TOKEN" in setup
    and "Set-Content" not in setup,
)
check(
    "privileged service is manual and receives a machine PATH snapshot",
    "obj= LocalSystem" in configure
    and "start= demand" in configure
    and "StartupType Manual" in configure
    and "SC220_RELEASE_RUNNER=1" in configure
    and "Join-Path $RunnerRoot '.env'" in configure,
)
check(
    "host verifier checks tools account and manual startup",
    "[Environment]::GetEnvironmentVariable('Path', 'Machine')" in verify
    and all(command in verify for command in ("git.exe", "gh.exe", "cmake.exe", "ninja.exe"))
    and "Visual Studio C++ Build Tools" in verify
    and "Inno Setup 6" in verify
    and "LocalSystem" in verify
    and "StartMode -ne 'Manual'" in verify
    and "Stop-Service" in verify,
)
check(
    "launcher rejects dirty or unpushed source",
    "git.exe status --porcelain" in start
    and "Local files are not clean" in start
    and "git.exe rev-parse FETCH_HEAD" in start
    and "Remote source ref" in start
    and "Type RELEASE to build, test, and publish" in start,
)
check(
    "launcher safely repairs only clean ASK-P submodule pointer drift",
    "Sync-CleanAskpSubmodule" in start
    and "submodule contains local edits" in start
    and "submodule update --init --recursive --force --checkout" in start,
)
check(
    "launcher can fast-forward a clean local source branch",
    "git.exe merge-base" in start
    and "git.exe merge --ff-only" in start
    and "Fast-forwarding automatically" in start,
)
check(
    "launcher separates stable workflow ref from build ref",
    "[string]$WorkflowRef = 'main'" in start
    and "contents/.github/workflows/local-release.yml?ref=$WorkflowRef" in start
    and "'--ref', $WorkflowRef" in start
    and '"source_ref=$SourceRef"' in start,
)
check(
    "launcher starts waits for and stops the privileged runner",
    "Start-And-WaitForRunner" in start
    and "Start-Service" in start
    and "Test-RunnerOnline" in start
    and "finally" in start
    and "Stop-LocalRunner" in start
    and "gh.exe run watch" in start,
)
check(
    "launcher reattaches to unfinished release instead of duplicating dispatch",
    "Get-ActiveWorkflowRun" in start
    and "Reusing it instead of dispatching a duplicate" in start
    and start.find("Get-ActiveWorkflowRun") < start.find("Invoke-GhCapture @workflowArguments"),
)
check(
    "launcher extracts workflow run ID directly from dispatch URL",
    "Get-RunIdFromDispatchOutput" in start
    and "/actions/runs/([0-9]+)" in start
    and "Workflow run ID received immediately" in start,
)
check(
    "launcher fallback uses monotonic numeric run IDs instead of DateTime casts",
    "Wait-ForNewWorkflowRun" in start
    and "databaseId -gt $PreviousRunId" in start
    and "[DateTime]$_.createdAt" not in start
    and "ToUniversalTime()" not in start,
)
check(
    "launcher verifies public tag before reporting success",
    "Test-PublicReleaseExists" in start
    and "completed without a verifiable public release tag" in start,
)
check(
    "release confirmation is case-insensitive but still explicit",
    "OrdinalIgnoreCase" in start
    and "Release cancelled by user" in start,
)
check(
    "publisher validates installer checksum and manifest",
    "Get-FileHash -LiteralPath $installer -Algorithm SHA256" in publish
    and "installer_sha256" in publish
    and "Manifest version" in publish,
)
check(
    "publisher targets only sc220-download",
    "[string]$TargetRepository = 'masarray/sc220-download'" in publish
    and "GH_TOKEN is required" in publish
    and "'release', 'create'" in publish,
)
check(
    "publisher updates website only after release asset verification",
    "Published release is missing assets" in publish
    and "Set-RepositoryTextFile" in publish
    and "-Path 'latest.json'" in publish
    and "RELEASE_NOTES_v$Version.md" in publish,
)
check(
    "same-version replacement is not enabled by workflow",
    "-ReplaceExisting" not in workflow
    and "Use an unused version or explicit replacement" in publish,
)
check(
    "command wrappers elevate only reviewed scripts",
    "fltmc" in setup_cmd
    and "setup-sc220-local-runner.ps1" in setup_cmd
    and "configure-sc220-runner-service.ps1" in setup_cmd
    and "verify-sc220-runner-host.ps1" in setup_cmd
    and setup_cmd.count("-ExecutionPolicy Bypass") == 4
    and "fltmc" in release_cmd
    and "start-sc220-local-release.ps1" in release_cmd
    and release_cmd.count("-ExecutionPolicy Bypass") == 2,
)

combined = "\n".join(text.values())
check(
    "repository contains no obvious literal credential",
    not re.search(
        r"(?:ghp_[A-Za-z0-9]{20,}|github_pat_[A-Za-z0-9_]{20,}|sk-[A-Za-z0-9]{20,})",
        combined,
    ),
)

failed = [name for name, ok in checks if not ok]
print("SC220 local self-hosted release audit")
for name, ok in checks:
    print(f"[ {'PASS' if ok else 'FAIL'} ] {name}")
print(f"Checks: {len(checks)}  Passed: {len(checks) - len(failed)}  Failed: {len(failed)}")
raise SystemExit(1 if failed else 0)
