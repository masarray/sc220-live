# Musical Noise Clean — DSP Design

## Objective

Reduce static analogue hiss and low-frequency noise in a live music signal without turning
cymbals, vocal air, ambience or reverb into watery speech-denoise artefacts.

The design combines moderate processors instead of one aggressive processor:

1. Gentle fixed subsonic cleanup.
2. Learned profile-based spectral attenuation.
3. Smooth stereo-linked downward expansion.

## Learn workflow

The user presses **Learn Noise** while the mixer/interface passes only its static noise. The
engine listens for two seconds and stores arithmetic mean power per FFT bin for left and right.
It also estimates the broadband time-domain noise floor for the expander threshold.

The profile is invalid while capture is in progress and after FFT quality/sample-rate changes.
The UI shows progress and then `Profile Learned`.

Future robustness work:

- Replace arithmetic mean with a trimmed mean or median estimator.
- Reject frames contaminated by transients or program audio.
- Persist a profile only when device, rate, quality and channel mode match.

## Low-frequency cleanup

Before the FFT, two cascaded one-pole high-pass stages operate around 24 Hz. This removes DC
and subsonic rumble with a gentle slope without requiring a large FFT merely for low-frequency
resolution. It is intentionally not an aggressive music high-pass filter.

Narrow 50/60 Hz hum and harmonics are not blindly notched in this milestone. A future adaptive
hum detector may add notches only when the learned profile proves stable tonal interference.

## Spectral reduction

The implementation uses a bounded Wiener-like gain estimate:

```text
threshold = learned_noise_power * threshold_offset
effective_snr = max(input_power / threshold - 1, 0)
wiener_gain = effective_snr / (effective_snr + 1)
target_gain = clamp(pow(wiener_gain, strength), minimum_gain, 1)
```

`minimum_gain` comes from the requested maximum reduction, so no bin is deleted.

### Frequency weighting

- Below about 18 Hz: spectral reduction disabled; subsonic stage owns this region.
- 18–80 Hz: stronger action for remaining rumble.
- 80 Hz–4 kHz: conservative to preserve musical body and vocal detail.
- 4–12 kHz: primary hiss-reduction region.
- Above 12 kHz: reduced action to preserve air.

The design avoids a fixed 6 kHz notch because that would remove real vocal presence, snare
attack and musical detail with the hiss.

### Protection

- Stereo-linked gains prevent image modulation.
- Local spectral peaks are protected as probable harmonics.
- Spectral flux quickly opens reduction for transients.
- Five-bin frequency smoothing reduces isolated chirps.
- Reduction engages slowly; musical detail releases quickly.

## Deterministic latency and aligned bypass

- Live: FFT 256 / hop 128.
- Clean: FFT 512 / hop 256.
- Square-root Hann with 50% overlap-add.
- Reported native latency equals the active FFT size.

A fixed dry delay uses the same latency. Noise Clean bypass therefore does not move audio in
time, and `Hear Removed` is computed as aligned dry minus wet rather than a comb-filtered mismatch.

Quality changes are applied during audio-device restart, never by rebuilding FFT/window state in
the callback. The prior profile is invalidated and must be learned again.

## Smooth expander

The expander is not a binary gate.

Default design range:

```text
Ratio:             1.0:1 at 0%, rising to about 1.75:1
Soft knee:         8 dB
Maximum range:     0 dB at 0%, rising to about 30 dB
Open time:         about 4 ms
Detector attack:   about 8 ms
Detector release:  about 85 ms
Hold:              about 85–145 ms
Close release:     about 450 ms
Hysteresis:        about 3 dB
```

Threshold follows the learned noise floor:

```text
close threshold = learned noise floor + threshold offset
open threshold  = close threshold + hysteresis
```

The detector uses cleaned pre-enhancer audio. Its stereo-linked gain is applied after the
Smart Enhancer, removing noise that enhancement raises without letting compressor/makeup
movement keep the expander open.

## Expected behaviour

- Music active: static hiss is moderately reduced, normally around 3–7 dB.
- Quiet tails: expander closes slowly and retains ambience.
- Silence: residual noise receives another 10–30 dB attenuation.
- New attack: expander opens quickly.
- Stereo: gain remains linked.

## Listening tests

Test with spoken/sung voice, full stereo music, cymbals, acoustic guitar attacks, long reverb,
sub-bass, real preamp hiss and hum/rumble.

Reject settings that produce watery/chirpy residuals, pumping with drums, missing consonants,
stereo movement, truncated reverb or audible open/close transitions.
