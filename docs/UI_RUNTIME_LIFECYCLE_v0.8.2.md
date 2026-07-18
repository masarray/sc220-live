# UI Runtime Lifecycle — v0.8.2

## Symptom

The audio engine reported running, but footer values stayed blank, the stereo meters did not move and the liquid sphere remained static.

## Root cause

`LiveMainComponent` started its timer while it was not yet attached to a visible top-level window. A construction-time `visibilityChanged()` path could stop the timer because `isShowing()` was false. JUCE does not guarantee a matching child visibility callback for every later parent-window show path, so the timer and visual tap could remain disabled.

## Fix

The application now explicitly calls:

```cpp
component->setUiActive(true);   // after showing the main window
component->setUiActive(false);  // before hiding to tray
```

Active mode starts the 24 Hz timer, enables the lock-free visual tap and performs an immediate status refresh. Inactive mode stops the timer and visual tap without stopping audio processing. `visibilityChanged()` remains only as a fallback for native window events.

## Realtime impact

No audio-callback work was added. The visual tap remains lossy and non-blocking. Hidden-to-tray operation remains lighter than visible mode.
