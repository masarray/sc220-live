# Implementation Status — Sonkupik Live v0.8.2

## Implemented

- Native Windows C++17/JUCE application and system-tray lifecycle.
- Input/output device selection with detailed settings in a CallOutBox.
- Before Enhancer and After Chain racks with real input/output faders and stereo meters.
- Profile-based Musical Noise Clean, fixed 256/512 transform and latency-aligned dry/listen-removed paths.
- Smooth downward expander and non-bypassable final safety limiter.
- Exact Smart Enhancer core and 40 presets from the locked `masarray/askp-vst` authority.
- Prepared-engine triple slots with background build and five-millisecond handover.
- Current ArSonKuPik-style time-domain liquid sphere with fixed resources, cached layers and tray sleep.
- Plus Jakarta Sans and Lucide-style embedded UX resources.
- Average callback load, callback peak, latency, dropouts and visual-drop telemetry.
- Visual Studio Community 2026 build detection and authority auto-sync.

## Deliberately not enabled in Live Safe

- External VST3 hosting. Rack slots remain architectural placeholders until scanning is isolated, state recall is reliable, plugin latency is reported and a watchdog can bypass a misbehaving plugin.
- Native split-clock audio bridge. The current JUCE device backend must be soak-tested with each interface/VB-CABLE combination.

## Acceptance still required on Windows

- MSVC Release build.
- 48 kHz / 128-sample test, then 256-sample fallback test.
- OBS/TikTok capture from VB-CABLE.
- Minimum eight-hour tray-mode soak test with zero audio dropouts.
- Callback peak target below 70% during normal use and below 85% during preset changes.

## v0.8.2 runtime correction

- Main workspace lifecycle is explicitly controlled by `setUiActive()`.
- Visible: 24 Hz status/meter/visual refresh and visual tap enabled.
- Hidden to tray: UI timer and visual tap disabled; audio processing remains active.
- Footer, meters and liquid sphere receive an immediate first refresh when the window opens.
