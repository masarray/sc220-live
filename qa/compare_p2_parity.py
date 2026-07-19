#!/usr/bin/env python3
import json
import math
import sys
from pathlib import Path


def fail(message: str) -> None:
    raise RuntimeError(message)


def main() -> int:
    if len(sys.argv) != 4:
        print("Usage: compare_p2_parity.py reference.json live.json report.txt", file=sys.stderr)
        return 2

    ref_path, live_path, report_path = map(Path, sys.argv[1:])
    reference = json.loads(ref_path.read_text(encoding="utf-8"))
    live = json.loads(live_path.read_text(encoding="utf-8"))

    lines = [
        "Sonkupik Live P2 preset-by-preset audio parity",
        f"Reference: {ref_path}",
        f"Live: {live_path}",
        "",
    ]

    for key in ("sampleRate", "blockSize", "durationSeconds", "analysisStartSeconds", "presetCount"):
        if reference.get(key) != live.get(key):
            fail(f"Render configuration differs for {key}: {reference.get(key)} vs {live.get(key)}")

    ref_presets = reference["presets"]
    live_presets = live["presets"]
    if len(ref_presets) != len(live_presets):
        fail(f"Preset count differs: {len(ref_presets)} vs {len(live_presets)}")

    failures = []
    for ref, candidate in zip(ref_presets, live_presets):
        identity = f"{ref['index']:02d} {ref['id']} / {ref['name']}"
        reasons = []
        for key in ("index", "id", "name", "latencySamples", "analysedSamples"):
            if ref[key] != candidate[key]:
                reasons.append(f"{key} {ref[key]!r} != {candidate[key]!r}")

        if ref["fnv1a64"] != candidate["fnv1a64"]:
            reasons.append(f"hash {ref['fnv1a64']} != {candidate['fnv1a64']}")

        rms_delta = abs(float(ref["rms"]) - float(candidate["rms"]))
        peak_delta = abs(float(ref["peak"]) - float(candidate["peak"]))
        if not math.isfinite(rms_delta) or rms_delta > 1.0e-12:
            reasons.append(f"rms delta {rms_delta:.3e}")
        if not math.isfinite(peak_delta) or peak_delta > 1.0e-12:
            reasons.append(f"peak delta {peak_delta:.3e}")

        if reasons:
            failures.append((identity, reasons))
            lines.append(f"FAIL  {identity}")
            lines.extend(f"      {reason}" for reason in reasons)
        else:
            lines.append(f"PASS  {identity}  {ref['fnv1a64']}")

    lines.extend([
        "",
        f"Presets checked: {len(ref_presets)}",
        f"Passed: {len(ref_presets) - len(failures)}",
        f"Failed: {len(failures)}",
        "Result: " + ("PASS" if not failures else "FAIL"),
    ])

    report_path.parent.mkdir(parents=True, exist_ok=True)
    report_path.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print("\n".join(lines[-5:]))
    return 1 if failures else 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(f"P2 parity comparison failed: {exc}", file=sys.stderr)
        raise SystemExit(3)
