# Key Engineering Decisions

1. **C++ + JUCE, not C# for the audio engine.** One native process reduces interop, GC and deployment complexity.
2. **No custom virtual audio driver.** Public kernel-driver signing cost is outside this non-profit project's direction.
3. **VB-CABLE is the output bridge.** The app remains responsible for processing, readiness and beginner UX.
4. **48 kHz is the reference rate.** It aligns common live-streaming software and VB-CABLE workflows.
5. **Musical denoise is profile based.** Speech AI suppression is not applied to a mixed stereo music bus.
6. **Denoise + expander are complementary.** Denoise works during program; expander completes silent gaps.
7. **Safety limiter stays last.** Users and future external plugins cannot move it behind an unsafe stage.
8. **Close means tray.** The audio service lifecycle is independent from the main window.
9. **External VST slots are visible but disabled in v0.6.0.** Stability gates take priority over feature theatre.
10. **Plus Jakarta Sans + Lucide icons.** The visual system is modern, coherent and premium without bulky typography.
11. **Smart Enhancer rebuild stays off the callback.** Triple prepared engine slots plus a short crossfade protect live audio from topology/vector rebuilds.
12. **Noise Clean bypass remains latency aligned.** A fixed dry delay prevents timing jumps and makes Hear Removed meaningful.
13. **FFT quality changes restart the device.** Reconfiguration and profile invalidation happen in `prepare()`, not during steady-state callback processing.
14. **A learned profile is configuration-specific.** Rate/FFT changes invalidate it; silently reusing mismatched bins is not allowed.

## 2026-07-11 — Windows build generator must be discovered, not pinned

**Decision:** The one-click builder detects the installed Visual Studio instance and available CMake generators. VS 2026 uses `Visual Studio 18 2026` only when CMake 4.2+ exposes it; otherwise the build uses Ninja inside the VS x64 developer environment.

**Reason:** Pinning `Visual Studio 17 2022` fails on machines that only have Visual Studio Community 2026. Blindly changing the string to VS 2026 also fails when an older CMake is first on PATH. Auto-discovery keeps the build reproducible without requiring users to understand generator/toolchain compatibility.

## 2026-07-11 — Main workspace follows signal flow, not device setup

**Decision:** The persistent workspace is divided into `Before Enhancer`, centre processing, and `After Chain`. Only input and output device selectors remain visible. Sample rate, buffer size, channel mode and denoise quality move to a settings callout.

**Reason:** Streamers need to understand where audio enters, what happens before/after enhancement, and where it exits. Infrequent technical configuration should not compete with live controls or meters.

## 2026-07-11 — One rotary control family for every musical macro

**Decision:** Reduction, Silence, Preserve, Enhance, Smart Bass, Smart Treble, Vocal Body and Stereo Magic all use the same compact `KnobControl`, diameter limits and `LiveLookAndFeel` renderer. Value text is repainted by a persistent slider listener during drag.

**Reason:** Mixed knob scales made the enhancer look secondary and caused stale readouts. A single control family creates a coherent professional VST identity and makes live adjustments trustworthy.

## 2026-07-11 — Live spectrum inherits the VST visual analysis strategy

**Decision:** The live analyzer uses logarithmic 80 Hz–16 kHz bands, band-peak aggregation, pink-noise visual compensation, VST attack/release constants, repeated five-tap spatial smoothing, a curved surface and a learned-profile overlay.

**Reason:** Raw short-FFT bins looked jagged and unrelated to the ArSonKuPik VST. Reusing its visual analysis principles gives the products one recognisable identity without changing the audio DSP.
