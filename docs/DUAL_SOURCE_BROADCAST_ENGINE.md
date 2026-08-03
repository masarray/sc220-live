# SC220 Live dual-source broadcast engine

## Purpose

SC220 Live must treat the Recording Tech SC220 voice lane and Windows PC Music lane as two real mixer channels. Each source receives processing appropriate to its role before both sources meet on the protected master bus.

## Implemented in this milestone

```text
SC220 / MIXER
  -> SC220 input trim
  -> Musical Noise Clean
  -> voice expander
  -> ASK-P Voice Signature Engine
  -> SC220 fader -------------------------------+
                                                  |
Windows default playback                          +-> master trim
  -> WASAPI loopback                              |   -> -1 dB master safety limiter
  -> ASK-P Music Signature Engine                 |   -> VB-CABLE stream output
  -> SC220-keyed ducking                          |
  -> PC Music fader ------------------------------+
```

The implementation now has two independent `EnhancerEngineBridge` instances:

- `voiceEnhancerBridge` for the SC220 lane;
- `musicEnhancerBridge` for PC Music.

Music enhancement happens before ducking. Voice activity therefore changes only the level of the already-enhanced music and does not alter the music enhancement character.

SC220 Input Trim is now confined to the voice lane. It no longer controls the shared ASK-P input gain after the sources have been mixed.

The former Dual Mono transport bug that duplicated channel 1 to both sides has also been removed. Both SC220 input channels remain present in the engine. Independent Dual Mono faders and pan controls remain a UI milestone.

## Master safety

Two individually protected source lanes can exceed full scale when summed. The new master bus therefore applies final output trim and a dedicated limiter with a `-1 dB` threshold before the stream output.

Telemetry now separates:

- voice Signature gain reduction;
- music Signature gain reduction;
- master limiter gain reduction;
- voice DSP latency;
- music DSP latency.

## Public control API prepared

The engine exposes independent music controls for the next UX pass:

- music preset selection;
- Music Enhance;
- Music Smart Bass;
- Music Smart Treble;
- Music Vocal Body;
- Music Stereo Magic;
- independent music bypass.

The existing preset and five macro methods remain mapped to the Voice Signature Engine to avoid breaking the current UI.

## Not completed in this milestone

The following items deliberately remain outside this first transport change:

1. independent physical Monitor Bus output;
2. Program / Music Cue monitor selection;
3. monitor feedback-loop protection;
4. adaptive PC Audio clock-drift correction;
5. PC endpoint selection instead of default System Mix only;
6. persistence and final UX controls for the Music Signature Engine;
7. real independent controls for SC220 Dual Mono channels;
8. Windows hardware validation and long-duration soak testing.

## Required validation

Before merge to `main`, run on Windows with the actual SC220 device detected as `MIXER`:

1. build Release with the pinned ASK-P submodule;
2. confirm voice-only processing;
3. confirm music-only processing and audible Music Signature changes;
4. confirm ducking occurs after music enhancement;
5. confirm voice Input Trim does not alter PC Music level;
6. confirm master output does not exceed the safety ceiling;
7. compare CPU callback load against the previous single-engine build;
8. run at least a one-hour stream test before starting monitor-client work.
