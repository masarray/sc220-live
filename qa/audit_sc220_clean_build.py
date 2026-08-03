from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
helper = (ROOT / "scripts/prepare-sc220-clean-build.ps1").read_text(encoding="utf-8")
one_click = (ROOT / "scripts/build-sc220-one-click.ps1").read_text(encoding="utf-8")

checks: list[tuple[str, bool]] = []


def check(name: str, condition: bool) -> None:
    checks.append((name, bool(condition)))


check(
    "clean helper only targets executables inside selected build tree",
    "StartsWith($buildPrefix" in helper
    and "Test-IsBuildExecutable" in helper
    and "ExecutablePath" in helper,
)
check(
    "running SC220 preview is stopped before build-tree removal",
    helper.find("Stop-BuildSC220Processes | Out-Null")
    < helper.find("Remove-Item -LiteralPath $build -Recurse -Force"),
)
check(
    "clean removal has bounded retry",
    "for ($attempt = 1; $attempt -le $MaxAttempts; $attempt++)" in helper
    and "Start-Sleep -Milliseconds (300 * $attempt)" in helper,
)
check(
    "clean helper reports likely lock owners",
    "Get-LikelyLockOwners" in helper
    and "Explorer preview, antivirus, or an indexing service" in helper,
)
check(
    "one-click invokes pre-clean helper only for Clean builds",
    "if ($Clean)" in one_click
    and "prepare-sc220-clean-build.ps1" in one_click,
)
check(
    "pre-clean executes before main build script",
    one_click.find("prepare-sc220-clean-build.ps1")
    < one_click.find("build-live-windows.ps1"),
)
check(
    "inner build does not receive a second Clean request",
    "$arguments.Clean" not in one_click,
)

failed = [name for name, ok in checks if not ok]
print("SC220 Live clean-build lock audit")
for name, ok in checks:
    print(f"[ {'PASS' if ok else 'FAIL'} ] {name}")
print(f"Checks: {len(checks)}  Passed: {len(checks) - len(failed)}  Failed: {len(failed)}")
raise SystemExit(1 if failed else 0)
