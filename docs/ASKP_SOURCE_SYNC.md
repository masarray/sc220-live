# ASK-P authority sync

SC220 Live uses `masarray/askp-vst` as a pinned private Git submodule at:

```text
third_party/askp-vst/
```

The current audio authority is the P0.3 development checkpoint:

```text
240b9d3be3ca1fbdedfa5b7027e465c22a56b0ea
```

The canonical ASK-P base engine at that checkpoint still reports v0.5.23 / DSP header revision `2026073102`. P0.3 has not yet been folded into canonical `engine/arsonkupik_dsp.cpp`, so SC220 Live does **not** compile that file directly. CMake runs the pinned upstream `scripts/generate_p0_musical_rebalance_dsp.py` and compiles the generated effective P0/P0.1/P0.2/P0.3 DSP together with the current 13-profile preset source.

For an existing checkout, initialise/repair the authority with:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\sync-askp-v0523.ps1
```

The historical script filename is retained for compatibility with local shortcuts. The script itself now verifies the P0.3 checkpoint gitlink.

The approved Sonkupik sphere remains independently owned by this repository under:

```text
third_party/sonkupik-sphere-v0.10.4/
```

ASK-P's editor and Orb visualizer are not compiled into SC220 Live.

Future engine/checkpoint upgrades must update the submodule gitlink, `ASKP_AUTHORITY.lock`, CMake authority/generator gates, CI checkout ref, bridge/runtime audits and the port documentation together. If upstream later folds P0.3 into canonical DSP, remove the generated-DSP build overlay only after parity/listening validation proves the canonical source is equivalent.
