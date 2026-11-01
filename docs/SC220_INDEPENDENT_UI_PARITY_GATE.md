# SC220 Live — UI parity / no-regression gate

The historical `live/SC220MainComponent.cpp`, `live/LiveTheme.cpp`, `docs/UX_SPEC.md` and `docs/SC220_LIVE_UX_LOCKED.png` are **visual design references** for the independent public VST3 host. The audio implementation must be decoupled from private ASK-P, **not the established product identity**.

## Mandatory product layout

- Preserve Recording Tech logo, SC220 LIVE identity, original Inter typography, graphite/metal palette, cyan operation states and amber warnings.
- Keep the professional **INPUT MIX | FX INSERT RACK / SIGNATURE | STREAM MASTER** layout. Do not replace it with a generic two-card debug GUI.
- Voice and Music have **three functioning serial VST3 slots each**, with Load, Edit, Bypass and Clear.
- Restore the five *real hosted VST3* macro controls (Enhance, Smart Bass, Smart Treble, Vocal Body, Stereo Magic). Never render a knob that is not bound to a public VST3 parameter.
- Show separate L/R input and master meters, vertical hardware-style faders, master output trim and explicitly safe-muted startup.
- Use bundled ArSonKuPik and ArSonVerb VST3 binaries from fixed upstream public release SHA256 values; never compile ASK-P into SC220.
- UI displays a visible error if a bundled plugin cannot be found, loaded or lacks signature parameters. Empty slots without a diagnosis are unacceptable.
- Do not hide absent PC loopback or legacy features behind decorative controls. Label missing functionality transparently.

## Quality gate

1. GitHub-hosted Windows MSVC Release compile must pass with **no private sources and no local runner**.
2. Both release VST3 packages must pass integrity checks and independent host instantiate/process smoke tests. Assert required signature parameter presence.
3. A physical tester should compare the full app at reference resolution against the locked original and verify logo, strip/fader proportions, meter colors, typography, spacing and no overlapping text.
4. Validate ASIO 48 kHz at 256, 128 and 64 samples where hardware permits, insert bypass/replacement and recording/stream routing. Report actual latency, never claim physical 0 ms.
5. Release as technical preview until **both feature/visual parity and hardware endurance** have been demonstrated.

The independent host must not be called production-stable merely because MSVC produced an EXE.
