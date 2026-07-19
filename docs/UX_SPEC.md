# UX Specification — Sonkupik Live v0.8.2

## Visual language

- Compact professional VST/DAW utility, not a bulky dashboard.
- Dark graphite/navy surfaces, fine borders, controlled cyan/violet accents and amber meters.
- Embedded Plus Jakarta Sans with restrained font sizes.
- Embedded Lucide-style line icons.
- Uniform compact knobs and real-time numeric readouts.

## Main workspace

### Header

Product name, engine/VB-CABLE status, settings, tray, minimise, maximise and close-to-tray controls.

### Left — Before Enhancer

Input Trim, Musical Noise Clean and reserved external VST slot, followed by input volume fader and stereo level indication.

### Centre top — Musical Noise Clean

Reduction, Silence and Preserve controls plus the ArSonKuPik liquid sphere as the visual hero. The sphere is not a technical spectrum graph; it expresses sub/body/air motion with fixed lightweight fluid physics.

### Centre bottom — Smart Enhancer

Factory preset selector, compact gain-reduction indication and uniform Enhance, Smart Bass, Smart Treble, Vocal Body and Stereo Magic knobs.

### Right — After Chain

Reserved external VST slot, Output Trim and locked Safety Limiter, followed by output fader and stereo meter.

### Footer

Average/peak callback load, total latency, dropouts, noise-profile state and tray readiness.

## Settings card

Sample rate, buffer size, channel mode, Musical Noise Clean quality, Hear Removed Noise, engine restart, device refresh and Learn Noise Again. These technical controls must not occupy the main workspace.

## Performance UX

- Main timer and visual tap are disabled when hidden to tray.
- The liquid stops repainting after it settles.
- Status widgets repaint only after meaningful value changes.
- No permanently animated decorative glow or particle field.

## v0.9.9 P0 premium workspace baseline

The authoritative P0 geometry and visual hierarchy are defined in:

- `P0_PREMIUM_LAYOUT_AUDIT_v0.9.9.md`
- `P0_LAYOUT_REDESIGN_BLUEPRINT_v0.9.9.md`

Future UI work must preserve the shared `WorkspaceLayout` model rather than creating separate paint/layout calculations. New spacing values should use `metrics` from `LiveTheme.h`.
