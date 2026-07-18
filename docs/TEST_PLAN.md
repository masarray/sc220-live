# Sonkupik Live Test Plan

## Functional

- Input selector lists capture devices.
- Output selector lists render devices.
- CABLE Input is detected as ready.
- Mono is duplicated consistently.
- Learn progress completes and profile state changes.
- Reduction, Silence and Preserve controls affect the correct processors.
- Presets update all five macro knobs.
- Bypass passes unenhanced audio.
- Engine off clears output.
- Close hides to tray and audio continues.
- Compact controller changes engine, bypass and preset state.
- Restart restores the selected devices.

## Audio quality

- Compare raw, denoise-only, expander-only and combined output.
- Listen to removed-noise audit mode.
- Confirm cymbals and reverb do not become watery.
- Confirm expander does not chatter between beats.
- Confirm silent noise reduction is significantly greater than active-program reduction.
- Confirm final peak stays below the safety ceiling.

## Performance

- 48 kHz / 64, 128, 256 and 512 samples where supported.
- Track CPU and dropout count while OBS encodes.
- Test integrated audio, common USB interfaces and VB-CABLE.
- Measure actual end-to-end latency with a loopback impulse; do not rely only on buffer arithmetic.

## Regression assets to add

- Static hiss WAV.
- Hum + hiss WAV.
- Stereo full music WAV.
- Voice + backing track WAV.
- Cymbal/reverb torture WAV.
- Expected meter and peak bounds.
