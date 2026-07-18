# Sonkupik Live v0.8.1 Build Fix

## Failure fixed

MSVC stopped in `live/MusicalNoiseClean.cpp` because `displayProfile` had already
been removed from the class during the v0.8.0 visualizer refactor, while one reset
loop still referenced it. The loop had no remaining runtime purpose and is removed.

## Additional cleanup

- `Display::userArea` is updated to JUCE 8 `userBounds`.
- Intentional cache-line padding in `VisualScopeTap` is retained and its MSVC
  warning is suppressed locally, not globally.
- `juce_audio_utils` is removed from the link list because the application uses
  `juce_audio_devices` directly. This avoids compiling the audio-processor hosting
  stack in the default Live Safe binary.

## Rebuild

Delete or clean `build-live`, then run:

```powershell
powershell -ExecutionPolicy Bypass `
  -File .\scripts\build-live-windows.ps1 `
  -Clean
```
