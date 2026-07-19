# Sonkupik Live v0.10.0 — P1 Premium Workspace Polish

## Objective

P1 takes the disciplined P0 geometry and improves the parts that still looked like separate widgets rather than one professional audio workstation surface. The implementation remains lightweight: no additional audio-thread work, no increased UI refresh rate, and no blur or continuously animated decoration.

## Implemented

### 1. Integrated side fader chassis

- Input and output faders now live in a dedicated inner mixer chassis.
- Label, numeric readout, scale, fader lane, thumb and stereo meter read as one module.
- The 0 dB reference receives a restrained cyan emphasis.
- Bypassed trim modules visually mute the fader and meter while remaining interactive; moving the fader reactivates the trim.
- The thumb uses a layered metallic face, centre groove, edge highlight and small hardware details.

### 2. Lightweight peak-hold stereo meters

- Added a UI-only peak hold of approximately 500 ms.
- Peak lines decay after the hold period without creating a new timer.
- The existing 24 Hz main UI timer remains the only refresh source.
- Audio callback metering and DSP are unchanged.

### 3. Premium rack rows

- Added active/bypass/add/safe state capsules.
- Added an accent rail and compact icon bay.
- Hover state uses existing mouse messages only; there is no animation timer.
- The entire row can toggle bypass, not only the small power icon.
- Bypassed modules mute their icon and text together.
- External slots remain honest ADD actions; locked safety protection remains non-bypassable.

### 4. Noise Clean visual balance

- Controls and visualizer now have separate inset bays inside the main card.
- Added a compact LIVE RESPONSE caption for the visual monitor.
- The expander monitor remains inside the control zone and no longer looks detached.
- The authoritative sphere visualizer and its 24 Hz policy are unchanged.

### 5. Smart Sound Enhancer control bay

- The preset selector now occupies a dedicated ACTIVE PRESET bay.
- Five macro controls share one inset control chassis.
- Subtle separators establish equal columns without large cards.
- The layout uses existing controls and adds no DSP or parameter polling.

### 6. Interaction polish

- Knob focus/hover slightly increases ring definition without changing geometry.
- Fader hover improves track and thumb definition.
- Disabled/bypassed controls preserve the same muted visual language introduced in P0.

## Intentionally deferred

- Rack reordering remains disabled until the actual external VST graph exists. A fake drag interaction would misrepresent the signal path.
- External VST scanning, crash isolation and plugin-latency compensation remain on the hosting roadmap.
- User-selectable workspace density and alternate themes remain P2 visual work.

## Real-time safety

P1 only changes JUCE component painting and message-thread interaction. It does not modify:

- `LiveAudioEngine::audioDeviceIOCallbackWithContext()`;
- the authoritative askp-vst processor;
- denoise FFT processing;
- expander coefficients;
- limiter, latency or gain staging;
- the 24 Hz timer frequency.
