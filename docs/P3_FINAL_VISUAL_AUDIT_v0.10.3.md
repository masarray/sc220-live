# P3 Final Visual Audit v0.10.3

## Root causes corrected

1. Tooltip renderer used a multi-line card inside JUCE's default one-line bounds.
2. Too many nested bays and borders made the interface look assembled rather than designed.
3. Momentary utility buttons were rendered as permanently active surfaces.
4. Rack rows combined rail, icon bay, index, status capsule and action surface with excessive visual weight.

## P3 changes

- explicit tooltip bounds through `getTooltipBounds()`
- compact structured tooltip title/body renderer
- reduced panel shadow and border count
- one visual surface for the response sphere, flat control areas elsewhere
- simplified preset row and macro area
- quiet header buttons at rest
- cleaner rack row icon and status hierarchy
- contextual tooltip microcopy for primary controls

No DSP, preset, audio callback, denoise, expander or limiter processing code was changed.
