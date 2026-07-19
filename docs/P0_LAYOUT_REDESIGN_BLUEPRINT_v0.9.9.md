# Sonkupik Live v0.9.9 — Premium Layout Redesign Blueprint

## Option B — final workspace blueprint

```text
┌──────────────────────────────────────────────────────────────────────────────┐
│ Logo  Sonkupik Live  │  LIVE ENGINE / ROUTING STATUS      utility controls │
├──────────────┬─────────────────────────────────────────────┬─────────────────┤
│ BEFORE       │ INPUT                          OUTPUT        │ AFTER CHAIN     │
│ ENHANCER     ├─────────────────────────────────────────────┤                 │
│              │ MUSICAL NOISE CLEAN                         │                 │
│ rack row 1   │ ┌──────── controls ────────┐ │ visualizer  │ rack row 1      │
│ rack row 2   │ │ 3 uniform knobs          │ │ sphere      │ rack row 2      │
│ rack row 3   │ │ profile / Learn           │ │             │ rack row 3      │
│              │ │ expander reduction meter │ │             │                 │
│ input fader  │ └───────────────────────────┘ │             │ output fader    │
│ + L/R meter  ├─────────────────────────────────────────────┤ + L/R meter    │
│              │ SMART SOUND ENHANCER                       │                 │
│              │ PRESET                                     │                 │
│              │  Enhance  Bass  Treble  Vocal  Stereo      │                 │
├──────────────┴─────────────────────────────────────────────┴─────────────────┤
│ CPU          TOTAL LATENCY       DROPOUTS       PROFILE       TRAY MODE     │
└──────────────────────────────────────────────────────────────────────────────┘
```

## Visual hierarchy

### Level 1 — operational state

- Product name and brand.
- Engine/routing status capsule.
- Input and output routing.

### Level 2 — core processing

- Musical Noise Clean.
- Smart Sound Enhancer.
- Bypass state per card.

### Level 3 — chain management

- Before Enhancer rack.
- After Chain rack.
- Input/output fader and meter modules.

### Level 4 — diagnostics

- CPU and callback peak.
- Total latency.
- Dropouts.
- Profile and tray state.

## Proportions at the 1440 × 860 reference size

```text
Header height              60 px
Footer height              48 px
Outer workspace padding    12 px
Column gutter              12 px
Left/right panel           ~240 px
I/O strip                  48 px
Noise Clean card           328–356 px
Card-to-card gap            10 px
Card internal padding      12–14 px
```

## Alignment rules

1. Every card title uses a 40–42 px header band.
2. Every card bypass button is centered in that same band.
3. All macro knob labels share one baseline.
4. All macro knob value readouts share one baseline.
5. The I/O labels use fixed widths so both dropdowns begin at matching coordinates.
6. Rack rows use the same height and vertical gap on both sides.
7. Footer columns are equal width.
8. Dividers are one physical pixel where possible.

## Colour and depth rules

- Primary surfaces use navy-charcoal gradients, not neutral black.
- Cyan is reserved for active controls and routing emphasis.
- Violet is reserved for musical/creative processing.
- Amber and red are reserved for level, reduction and warning information.
- White highlights stay below approximately 6% opacity.
- Shadows remain local and subtle; no large glow fields.

## Typography rules

- Plus Jakarta Sans remains the product typeface.
- Section titles: compact semibold, never oversized.
- Field labels: uppercase and muted.
- Readouts: brighter than labels but smaller than section titles.
- No large display typography inside the processing workspace.

## Interaction rules

- Utility buttons stay visually quiet when idle.
- Toggle buttons make active/bypassed state immediately visible.
- Disabled controls lose arc saturation, text brightness and marker contrast together.
- Tooltips use one premium callout language.
- No UI action may trigger work on the real-time audio callback beyond atomic parameter updates.
