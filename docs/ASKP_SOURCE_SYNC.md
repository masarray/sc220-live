# ASK-P authority sync

SC220 Live uses `masarray/askp-vst` as a pinned private Git submodule at:

```text
third_party/askp-vst/
```

## Approved production authority

For the SC220 Live v0.1.1 release line:

- latest audited ASK-P repository HEAD: `44b7d1657fbbd2c13cd490e251e8867734238623`;
- approved v0.5.24 product source commit: `2c764f82b0fc99eaff04d918cef4f839db7509e5`;
- approved release-helper commit: `811b7376dd784ce9f64e03512c7baf7de5e77425`;
- DSP header revision: `2026073102`;
- factory profiles: `13`.

SC220 Live pins the **approved product source commit**, not the moving repository HEAD. The final upstream HEAD commit after the approved product source only repins release metadata; the approved product source itself contains the latest Gain Match settle-controller implementation and the matching source-audit contract.

The core tonal DSP/preset blobs and P0/P0.1/P0.2/P0.3 generator remain the same approved v0.5.24 sonic calibration used by the preceding SC220 port. The processor-level difference that arrived after the older `1cf050d4...` pin is the Gain Match settle-controller fix and its source-audit alignment.

## Production DSP contract

ASK-P v0.5.24 production keeps the canonical `engine/arsonkupik_dsp.cpp` as immutable input and materialises the approved effective DSP through a fail-closed sequence:

1. run `tools/p0_musical_rebalance_source_audit.py`;
2. refuse configuration if a sonic invariant or generator replacement anchor has drifted;
3. run `scripts/generate_p0_musical_rebalance_dsp.py`;
4. compile the generated DSP translation unit for product and parity targets.

SC220 Live mirrors this ordering. Its additional generated-DSP anchors supplement, rather than replace, the complete upstream audit.

## Existing checkout sync

Run:

```powershell
.\scripts\sync-askp-v0523.ps1
```

The script filename is historical and retained for compatibility. Its current authority is ASK-P **v0.5.24 @ `2c764f82...`**.

Normal one-click builds also invoke this authority sync before CMake, preventing a clean local checkout from silently compiling a stale submodule pointer.

For a raw Git repair:

```powershell
git submodule sync -- third_party/askp-vst
git submodule update --init --recursive --force --checkout third_party/askp-vst
git -C third_party/askp-vst rev-parse HEAD
```

The final command must print:

```text
2c764f82b0fc99eaff04d918cef4f839db7509e5
```

## SC220-specific boundaries

- local visualizer authority remains `third_party/sonkupik-sphere-v0.10.4/`;
- ASK-P editor and ASK-P Orb visualizer are not compiled into SC220 Live;
- Live macro glide remains 12 ms, reference glide remains 80 ms;
- Gain Match source/runtime logic is present for processor parity, but `EnhancerEngineBridge` keeps it disabled by default and the SC220 UI does not expose it.

Future ASK-P upgrades must update together: submodule gitlink, `ASKP_AUTHORITY.lock`, CMake source contract, source-audit workflow, release workflows, bridge metadata/defaults, QA, and port documentation.
