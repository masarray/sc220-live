# Sonkupik Live v0.7.0 Layout Audit

## Problems observed in the v0.6.2 workspace

1. **Encoding noise / mojibake**
   - UTF-8 punctuation was compiled through the active Windows code page.
   - Status lines and popup entries could render as `A`, `Â`, or broken symbols.
   - Mixed Unicode punctuation added no functional value to the UI.

2. **Weak information hierarchy**
   - The left side was used as a technical setup form rather than an audio signal-flow surface.
   - The centre bottom rack duplicated information already represented by the built-in processing modules.
   - The right panel mixed output routing, meters, engine power, bypass and mute into one dense column.

3. **Spectrum did not match the VST identity**
   - It displayed a direct 128-point trace with a coarse, artificial shape.
   - Frequency spacing was visually linear rather than the VST's musical logarithmic view.
   - It lacked the VST visual calibration, smoothing and fluid line treatment.

4. **Knob inconsistency**
   - Noise-clean knobs and enhancer knobs used the same LookAndFeel but were laid out at very different sizes.
   - Enhancer controls became visually tiny and looked like a different product family.
   - Parent value labels were not explicitly repainted while the child slider was dragged.

5. **Main workspace exposed setup details too early**
   - Sample rate, buffer size and channel mode competed with the musical controls.
   - These are important but infrequent settings and belong in a focused settings card.

## v0.7.0 target hierarchy

```text
Header: Sonkupik Live + unambiguous engine state + window actions

Left / BEFORE ENHANCER
  Input Trim
  Musical Noise Clean
  External VST Slot
  Input volume fader + stereo indicator

Centre header
  INPUT dropdown                         OUTPUT dropdown

Centre upper
  Musical Noise Clean controls + learned profile state
  Wide VST-derived logarithmic spectrum

Centre lower
  Mas Ari Signature preset
  Gain reduction strip
  Five uniform Smart Enhancer knobs

Right / AFTER CHAIN
  External VST Slot
  Output Trim
  Locked Safety Limiter
  Output volume fader + stereo indicator

Footer
  CPU / total latency / dropouts / profile / tray mode
```

## Implemented decisions

- Product-facing name changed to **Sonkupik Live**.
- Main workspace now exposes only input and output device dropdowns.
- Sample rate, buffer size, channel mode and denoise quality moved to a `CallOutBox` settings card.
- Left and right columns now represent real signal flow before and after the enhancer.
- Input and output faders are connected to real engine trim parameters.
- Output trim is applied before the locked safety limiter so positive trim cannot violate the output ceiling.
- All eight rotary controls use the same compact `KnobControl` geometry and visual family.
- Knob readouts repaint from a persistent `Slider::Listener`, so values track the drag continuously.
- Spectrum display now uses the VST strategy: 80 Hz to 16 kHz log bands, band peak aggregation,
  pink-noise visual compensation, fast attack, slow release, two-pass spatial smoothing, curved path,
  cyan glow/fill and learned-profile overlay.
- UI source strings are ASCII-safe and MSVC is explicitly compiled with `/utf-8`.
