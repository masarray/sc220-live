# SC220 Live Development Context

Current published milestone: **SC220 Live v0.1.1 — 10 August 2026**.

The exact private source commit published as v0.1.1 is:

```text
994b983a7f02b6a2285c41781ecb77dc07e3b217
```

Documentation commits after that source commit do not change the already-published v0.1.1 binary provenance.

## Audio authority

- ASK-P engine version: `0.5.24`
- Approved ASK-P product source: `2c764f82b0fc99eaff04d918cef4f839db7509e5`
- Latest audited upstream HEAD for this release line: `44b7d1657fbbd2c13cd490e251e8867734238623`
- DSP header revision: `2026073102`
- Factory profiles: `13`
- Integration: pinned private Git submodule, headless processor, fail-closed upstream source audit, deterministic P0/P0.1/P0.2/P0.3 generated DSP

## Intentional SC220 Live delta

- ASK-P editor excluded.
- ASK-P Orb visualizer excluded; local `sonkupik-sphere-v0.10.4` remains the visual authority.
- Creative macro smoothing is 12 ms for Live; the parity/reference processor remains 80 ms.
- Gain Match processor logic is compiled for authority parity, but Gain Match is **disabled by default** and **not exposed in the SC220 Live UI**.
- SC220-specific licence storage, checkout product identity, audio routing, WASAPI capture, ducking, Monitor Bus, and programme protection remain owned by SC220 Live.

## Architecture boundary

`LiveAudioEngine` communicates with ASK-P through `EnhancerEngineBridge`. ASK-P parameter IDs, preset handoff, safety telemetry, Gain Match telemetry, and DSP-specific access remain inside that adapter boundary.

## Public release authority

- Public repository: `masarray/sc220-download`
- Stable version: `v0.1.1`
- Installer SHA-256: `589b174357cb36c62611d7a0e89bfa6a7d25251fc27015b5dd5b25fb54c04686`
- Public publish/verification workflow run: `31346714396` (`Publish verified SC220 draft release`) — success

The public release workflow verifies and publishes staged compiled assets. It must not be confused with private source/runtime validation evidence. Keep build, listening, and device-test evidence tied to the exact release source commit.

## Development rules

1. Do not follow the moving ASK-P branch head directly; use the approved source named by `ASKP_AUTHORITY.lock`.
2. Any ASK-P upgrade must update the submodule gitlink, lock file, CMake source contract, CI checkout refs, QA anchors, bridge metadata, and documentation together.
3. Do not expose Gain Match in SC220 Live merely because ASK-P contains it; that is a separate product/UI decision.
4. Keep public distribution binary-only and preserve the private/public repository boundary.
5. Record every public release with an immutable source commit and installer SHA-256 receipt.
