# Enhancer Engine Port — Sonkupik Live v0.12.0

## Authority

Sonkupik Live v0.12.0 embeds the reviewed ArSonKuPik v0.5.19 Signature Engine
from the supplied `askp-vst-master (4).zip` archive.

- Engine version: 0.5.19
- DSP header revision: 2026072702
- Factory profiles: 13
- Source archive SHA-256: `bb28592ba9b009dfe4f3ef6c7056ae6eee8e94a1db3e1e88ba6ba68a1a73931d`

## Refactor

`LiveAudioEngine` no longer accesses APVTS IDs or processor internals directly.
`EnhancerEngineBridge` is now the only integration boundary for:

- preparation and processing;
- latency reporting;
- factory profile selection;
- five creative macros;
- input/final-output utility gain;
- bypass and Gain Match policy;
- meters and visual ScopeTap.

This keeps future engine migrations local to one adapter.

## Live-only deltas

The port intentionally changes only two processor integration details:

1. VST editor is removed (`hasEditor() == false`, `createEditor() == nullptr`).
2. Creative macro glide is 12 ms instead of the VST's 80 ms host-automation glide.

The DSP engine, 13 profile definitions, generated Signature scene lanes, Gain
Match implementation, final lookahead safety, output ceiling and preset macro
values are copied from the supplied v0.5.19 source.

## Visual isolation

The ASK-P `OrbVisualizer` from v0.5.19 is **not** imported. Sonkupik Live uses a
local snapshot of the approved v0.10.4 sphere and metallic navy-black bezel.
A clean build therefore cannot regress when another `askp-vst` checkout changes.

## Build determinism

CMake verifies SHA-256 hashes for the embedded engine, processor port, preset
bank and local sphere. A mismatch is a fatal configure error rather than a
warning.
