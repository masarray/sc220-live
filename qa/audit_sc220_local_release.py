from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]

workflow_path = ROOT / ".github" / "workflows" / "local-release.yml"
setup_path = ROOT / "scripts" / "setup-sc220-local-runner.ps1"
verify_host_path = ROOT / "scripts" / "verify-sc220-runner-host.ps1"
start_path = ROOT / "scripts" / "start-sc220-local-release.ps1"
publish_path = ROOT / "scripts" / "publish-sc220-download.ps1"
setup_cmd_path = ROOT / "SETUP-LOCAL-RUNNER.cmd"
release_cmd_path = ROOT / "RELEASE-SC220.cmd"

required_paths = (
    workflow_path,
    setup_path,
    verify_host_path,
    start_path,
    publish_path,
    setup_cmd_path,
    release_cmd_path,
)

workflow = workflow_path.read_text(encoding="utf-8")
setup = setup_path.read_text(encoding="utf-8")
verify_host = verify_host_path.read_text(encoding="utf-8")
start = start_path.read_text(encoding="utf-8")
publish = publish_path.read_text(encoding="utf-8")
setup_cmd = setup_cmd_path.read_text(encoding="utf-8")
release_cmd = release_cmd_path.read_text(encoding="utf-8")

checks: list[tuple[str, bool]] = []


def check(name: str, condition: bool) -> None:
    checks.append((name, bool(condition)))


check("all one-click release files exist", all(path.exists() for path in required_paths))
check(
    "release workflow is manual-only",
    "workflow_dispatch:" in workflow
    and not re.search(r"(?m)^\s{2}(?:push|pull_request|pull_request_target|schedule):", workflow),
)
check(
    "workflow is pinned to the repository-only SC220 Windows label",
    "runs-on: [self-hosted, Windows, X64, sc220-build]" in workflow,
)
check(
    "workflow uses stock Windows PowerShell",
    "shell: pwsh" not in workflow
    and workflow.count("shell: powershell") >= 6,
)
check(
    "workflow serializes public releases",
    "group: sc220-live-local-public-release" in workflow
    and "cancel-in-progress: false" in workflow,
)
check(
    "source and private authority checkouts do not persist credentials",
    workflow.count("persist-credentials: false") >= 2,
)
check(
    "ASK-P authority remains pinned",
    "repository: masarray/askp-vst" in workflow
    and "5757119cab7a1a75f09dcf0edea6e3be97975439" in workflow,
)
check(
    "public release token is exposed only to the publish step",
    workflow.count("SC220_DOWNLOAD_TOKEN") == 1
    and "GH_TOKEN: ${{ secrets.SC220_DOWNLOAD_TOKEN }}" in workflow
    and not re.search(r"(?m)^\s{4}GH_TOKEN:", workflow),
)
check(
    "workflow never stores a GitHub-hosted artifact",
    "actions/upload-artifact" not in workflow
    and "actions/download-artifact" not in workflow,
)
check(
    "smoke install avoids changing VB-CABLE drivers",
    '/TASKS="!vbcable,!desktopicon"' in workflow
    and "SC220PrerequisiteProbe.ps1" in workflow
    and "unins000.exe" in workflow,
)
check(
    "release is published only after build audit and smoke test",
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
    "one-click setup installs an official repository runner as a service",
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
    "runner download digest is verified when GitHub supplies it",
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
    "post-setup verifier checks service-level machine tools",
    "[Environment]::GetEnvironmentVariable('Path', 'Machine')" in verify_host
    and all(command in verify_host for command in ("git.exe", "gh.exe", "cmake.exe", "ninja.exe"))
    and "Visual Studio C++ Build Tools" in verify_host
    and "Inno Setup 6" in verify_host
    and "actions.runner.*" in verify_host,
)
check(
    "release launcher refuses dirty or unpushed source",
    "git.exe status --porcelain" in start
    and "Local files are not clean" in start
    and "git.exe rev-parse FETCH_HEAD" in start
    and "Remote source ref" in start
    and "Type RELEASE to build, test, and publish" in start,
)
check(
    "release launcher uses a stable workflow ref and separate build ref",
    "[string]$WorkflowRef = 'main'" in start
    and "contents/.github/workflows/local-release.yml?ref=$WorkflowRef" in start
    and "'--ref', $WorkflowRef" in start
    and '"source_ref=$SourceRef"' in start,
)
check(
    "release launcher verifies the local runner before dispatch",
    "actions/runners?per_page=100" in start
    and "sc220-build" in start
    and "'workflow', 'run'" in start
    and "gh.exe run watch" in start,
)
check(
    "publisher validates installer checksum and manifest",
    "Get-FileHash -LiteralPath $installer -Algorithm SHA256" in publish
    and "installer_sha256" in publish
    and "Manifest version" in publish,
)
check(
    "publisher targets only the public distribution repository",
    "[string]$TargetRepository = 'masarray/sc220-download'" in publish
    and "GH_TOKEN is required" in publish
    and "'release', 'create'" in publish,
)
check(
    "publisher updates website metadata after a verified release",
    "Set-RepositoryTextFile" in publish
    and "-Path 'latest.json'" in publish
    and "RELEASE_NOTES_v$Version.md" in publish
    and "Published release is missing assets" in publish,
)
check(
    "same-version replacement is not enabled by the workflow",
    "-ReplaceExisting" not in workflow
    and "Use an unused version or explicit replacement" in publish,
)
check(
    "command wrappers bypass policy only for reviewed local scripts",
    "setup-sc220-local-runner.ps1" in setup_cmd
    and "verify-sc220-runner-host.ps1" in setup_cmd
    and "start-sc220-local-release.ps1" in release_cmd
    and setup_cmd.count("-ExecutionPolicy Bypass") == 2
    and release_cmd.count("-ExecutionPolicy Bypass") == 1,
)

combined = "\n".join(
    (workflow, setup, verify_host, start, publish, setup_cmd, release_cmd)
)
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
