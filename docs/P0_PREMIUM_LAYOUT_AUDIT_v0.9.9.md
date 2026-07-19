# Sonkupik Live v0.9.9 — P0 Premium Layout Audit

## Objective

Raise the main workspace from a functional dark dashboard to a coherent premium audio workstation surface. The target is not to imitate a DAW timeline; it is to reach the visual discipline, alignment, hierarchy and interaction quality expected from top-tier professional audio software.

## Option A — implementation checklist

### P0.1 One authoritative layout calculation

**Problem before v0.9.9**

`paint()` and `resized()` independently reconstructed the workspace. Small differences in padding and removed rectangles caused labels, controls, separators and cards to drift out of alignment.

**Implemented**

- Added one `WorkspaceLayout` model.
- `makeWorkspaceLayout()` now produces every important rectangle.
- `paint()` and `resized()` consume the same geometry.
- Header, side racks, I/O strip, Noise Clean, Smart Sound Enhancer and footer now share the same grid.

**Acceptance**

- No control is positioned using a second approximation of the card geometry.
- Section title baselines and bypass buttons use the same header rectangles.
- Input and output fields use one symmetrical I/O strip.

### P0.2 Eight-point spacing system

**Problem before v0.9.9**

The interface used unrelated values such as 7, 10, 14 and 22 pixels without a common system. The result looked assembled rather than designed.

**Implemented**

Added shared metrics in `LiveTheme.h`:

```text
2 / 4 / 6 / 8 / 10 / 12 / 14 / 16 / 20 / 24 px
```

Primary structural values:

```text
Header          60 px
Footer          48 px
Side panel      up to 240 px
Rack row        48 px
Card radius     12 px
Control radius   8 px
```

### P0.3 Header hierarchy and window controls

**Problem before v0.9.9**

- Engine status was small floating text.
- All utility buttons looked like permanent dark squares.
- Some titlebar SVG resources could appear blank.
- SVGs were parsed again during every repaint.

**Implemented**

- Engine state is now a compact status capsule.
- Momentary utility buttons show surfaces only on hover/press.
- Toggle power buttons keep a quiet active state.
- Active and inactive Lucide drawables are cached.
- Manual lightweight fallback glyphs cover minimise, maximise, tray, close, plus and power if an SVG resource is unavailable.
- Brand logo, title, separator and status pill form one horizontal hierarchy.

### P0.4 I/O strip

**Problem before v0.9.9**

Input and output selectors floated directly over the workspace and did not read as one routing section.

**Implemented**

- Added a single low-profile premium I/O surface.
- Input and output halves use identical label widths, vertical alignment and internal padding.
- The strip follows the same radius and border language as the processing cards.

### P0.5 Musical Noise Clean balance

**Problem before v0.9.9**

- Three knobs, profile controls, a thin expander meter and the sphere felt like separate fragments.
- The expander meter was too tall and visually weak.
- No consistent division existed between controls and visual monitor.

**Implemented**

- Controls, expander monitor and visualizer are now three deliberate zones.
- Added a subtle vertical divider between control and monitor areas.
- Expander meter is a compact premium reduction module.
- Meter convention is now `0 dB` at the top and `-24 dB` at the bottom.
- Current reduction uses a moving marker and amber-to-red attenuation fill.
- Readout shows the actual negative gain reduction.
- Profile and Learn controls align to the same left edge and width.

### P0.6 Smart Sound Enhancer preset hierarchy

**Problem before v0.9.9**

The preset selector was visually detached and the enhancer card contained a large undefined empty strip.

**Implemented**

- Added a dedicated `PRESET` caption with a small cyan anchor line.
- Preset combo receives a fixed premium width.
- Five macro knobs use one equal-column grid.
- Card title, preset strip, knob labels and value readouts now have explicit vertical bands.
- Bypass remains in the card header and does not compete with the preset strip.

### P0.7 Card surfaces

**Problem before v0.9.9**

Cards were readable but flat and mechanically separated.

**Implemented**

- Refined panel gradient.
- Added restrained inner highlight.
- Added one-pixel top highlight and precise outer border.
- Added subtle header separator lines.
- Kept contrast intentionally restrained to avoid a game-dashboard appearance.

### P0.8 Lightweight rendering

- No animation rate was increased.
- No blur filters or per-frame image effects were added.
- Header icons are cached rather than reparsed every frame.
- Layout rectangles are calculated once per `paint()` or `resized()` call, without heap allocation.
- The existing 24 Hz UI refresh policy remains unchanged.

## Deferred to P1

- More refined side-panel fader chassis and meter integration.
- Rack-row drag handles and insertion feedback.
- More detailed hover/focus animation.
- Responsive typography below the current minimum window size.

## Deferred to P2

- User-selectable compact workspace density.
- Theme variants and accent selection.
- Advanced VST rack editor visuals.
