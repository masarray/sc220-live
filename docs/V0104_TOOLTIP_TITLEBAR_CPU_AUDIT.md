# Sonkupik Live v0.10.4 Audit

## Findings

1. Tooltip white rectangle
   - The custom tooltip used a partially transparent surface on Windows.
   - A resize or backing-surface refresh could expose the default OS/JUCE background.
   - Fix: use an opaque TooltipWindow and paint every pixel before drawing the premium surface.

2. Small tray/minimise/maximise icons
   - Missing SVG assets used the fallback renderer.
   - The fallback received bounds that had already been reduced, then reduced them again.
   - Fix: pass full button bounds to the fallback renderer, matching the Close icon proportion.

3. Fast-changing CPU text
   - The UI footer formatted CPU numbers on every 24 Hz UI refresh.
   - Fix: retain internal full-rate measurements, but publish a smoothed display value once per second.

## Scope safety

No changes were made to:
- LiveAudioEngine.cpp
- MusicalNoiseClean.cpp
- askp-vst DSP and presets
- audio callback timing
- meter and visualizer signal publication
