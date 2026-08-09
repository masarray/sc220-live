# ASK-P authority sync

SC220 Live uses `masarray/askp-vst` as a pinned private Git submodule at:

```text
third_party/askp-vst/
```

## Approved production authority

The latest audited ASK-P repository HEAD is:

```text
f91aa04e953cf14eb0682a1bc305138b6507d73a
```

Its approved v0.5.24 release request still names this exact **product source commit**:

```text
1cf050d40338cbcb5d8cddd9fc2de5f23e3d625f
```

The current approved release-helper commit is `811b7376dd784ce9f64e03512c7baf7de5e77425`.

SC220 Live pins the submodule to the approved product source commit rather than following the moving repository HEAD. Commits after `1cf050d4...` currently change release-request, release-audit, macOS/Pages setup and verification helpers; the product DSP, presets, processor and production CMake source contract are unchanged.

ASK-P product metadata is now **v0.5.24** with DSP header revision `2026073102` and 13 factory profiles.

## Production DSP contract

v0.5.24 intentionally keeps the large canonical `engine/arsonkupik_dsp.cpp` as immutable input and builds the approved P0/P0.1/P0.2/P0.3 sound through a deterministic fail-closed production sequence:

1. run `tools/p0_musical_rebalance_source_audit.py`;
2. refuse configuration if any sonic invariant or replacement anchor has drifted;
3. run `scripts/generate_p0_musical_rebalance_dsp.py`;
4. compile the generated DSP translation unit for the product and parity targets.

SC220 Live now mirrors the same ordering. Its own small generated-DSP anchor checks are supplemental only; the complete upstream source audit remains authoritative.

For an existing checkout, initialise/repair the authority with:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\sync-askp-v0523.ps1
```

The historical script filename is retained for compatibility with existing local shortcuts. `scripts/build-live-windows.ps1` also invokes this sync helper automatically before configuring CMake, so normal one-click local builds cannot silently use a stale ASK-P checkout.

The approved Sonkupik sphere remains independently owned by this repository under:

```text
third_party/sonkupik-sphere-v0.10.4/
```

ASK-P's editor and Orb visualizer are not compiled into SC220 Live.

Future ASK-P upgrades must update together: the approved source gitlink, `ASKP_AUTHORITY.lock`, CMake audit/generator contract, CI checkout ref, bridge version/defaults, sync/build helpers, runtime audits and port documentation.
