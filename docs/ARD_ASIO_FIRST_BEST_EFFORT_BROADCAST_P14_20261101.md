# ARD — ASIO-First, Best-Effort Broadcast (P14)

Status: implementation contract, 2026-11-01. Branch: `feature/original-ui-studio-one-fx-racks`, parent PR #14.

## Decision
The singer's feedback/recording headset uses the **same physical ASIO interface** for native input and output. Post-FX stereo programme is fanned out into an independent Windows WASAPI Shared Mode render endpoint (typically `CABLE Input`). OBS/TikTok reads `CABLE Output`. Optional physical Windows/WASAPI auxiliary monitor remains independent and is never the singer's required monitor.

### Block model
```text
              HARD REALTIME / ASIO SAMPLE CLOCK
     ASIO IN -> VOICE FX --+
     PC LOOPBACK -> MUSIC FX +- DUCK -> MASTER TRIM -> LIMITER -> programme
                                                                |--> ASIO OUT 1/2 (singer; unconditional)
                                                                |--> SPSC COPY (bounded, no wait) -> broadcast worker -> WASAPI SHARED CABLE Input -> OBS
                                                                \--> SPSC COPY -> optional physical WASAPI aux monitor
              INDEPENDENT WINDOWS ENDPOINT CLOCK(S)
```
The tee uses the completed programme buffer *before* callback return. No ASIO secondary device, extra ASIO buffer, pipeline reordering, or second DSP graph is allowed.

## Architecture and ownership
- `juce::AudioDeviceManager`: sole ASIO input/output device and callback; sample clock authority. No broadcaster may invoke `setAudioDeviceSetup`, change ASIO sample rate/buffer, or become a JUCE callback.
- `MonitorOutputClient(physicalOnly)`: existing auxiliary physical WASAPI monitor; keep PROGRAM/MUSIC CUE, independent mute and gain. This is NOT the performer's reference.
- `MonitorOutputClient(virtualCableOnly)` held as `broadcastOutput`: **separate worker instance** (own render endpoint, SPSC FIFO, resampling state, error reporting), restricted to virtual playback `CABLE Input`. Operationally a distinct broadcast bus, sharing a reusable endpoint transport rather than duplicating DSP.
- `GO LIVE`/`outputMuted`: publish/mute broadcast copy, **never mute the native ASIO output**. No physical headphone fader/monitor clock is controlled by this boolean.
- GUI `outputBox` is broadcast WASAPI endpoint selection; ASIO I/O, block length and rate stay in the Audio Setup dialog. Persist `broadcastDevice` separately from `monitorDevice` and the JUCE ASIO device state.
- Physical Aux WASAPI must remain filtered away from VB-CABLE; Broadcast WASAPI must accept only virtual playback endpoints, never Windows capture endpoint or physical speaker.

## Callback invariant
```cpp
processVoiceAndMusicDsp();
processMasterLimiter();
if (goLive && broadcastReady) broadcastSpsc.tryPush(programme); // bounded copy/drop
for (auto ch : asioOutputChannels) copy(programme[ch], asioOut[ch]); // unconditional
```
- No COM, WASAPI device calls, locks that may block, allocations, thread start/stop, sleeps, filesystem, condition waits or clock drift correction in ASIO callback.
- FIFO is SPSC: ASIO is sole producer, WASAPI worker sole consumer; never modify consumer index from producer. On full FIFO, drop the *new* broadcast block and increment overflows; do not wait and do not corrupt existing frames.
- Selection/reconnect must gate publishing safely; the producer cannot wait on the worker. Recovery occurs asynchronously in the secondary worker. An inactive sink never mutates ASIO outputs.
- Downstream resampling, endpoint native format conversion, drift/adaptive ratio and WASAPI buffer scheduling stay entirely in the worker. The Windows system clock may differ from the ASIO audio-interface crystal.
- Broadcast worker priority normal (lower than hardware realtime); auxiliary WASAPI is optional, never required to keep ASIO running.

## Relevant established techniques
- JUCE high-priority audio callback: https://docs.juce.com/master/classjuce_1_1AudioIODeviceCallback.html
- Microsoft WASAPI shared render/event buffer model: https://learn.microsoft.com/en-us/windows/win32/coreaudio/rendersharedeventdriven
- Windows clock drift synchronization principles: https://learn.microsoft.com/en-us/windows-hardware/drivers/audio/clock-synchronization
- Windows MMCSS audio scheduling: https://learn.microsoft.com/en-us/windows/win32/coreaudio/user-mode-audio-components
- OBS virtual cable input capture: https://obsproject.com/kb/application-audio-capture-guide
- VB-CABLE reference: https://vb-audio.com/Cable/VBCABLE_ReferenceManual.pdf

## Safety and residual risks
ASIO hardware/driver latency and VST processing latency are nonzero; this work **adds zero ASIO buffering stages**, not guaranteed zero end-to-end milliseconds. The bounded FIFO copy still consumes some CPU/memory bandwidth. If the machine is overloaded, both threads may suffer scheduling jitter, so “zero ASIO XRUN” is an acceptance measurement, not a universal warranty. Fixed-size FIFO and worker recovery must be checked under CPU pressure and Windows suspend/device unplug.

Never feed `CABLE Output` into PC MUSIC loopback: that would feed the post-FX programme back into itself.

## Validation / release gate
1. Windows MSVC, CTest, generated P0–P14 anchors all pass; no paid activation regression.
2. 48 kHz ASIO @ 256 / 128 / 64 samples; baseline broadcast OFF versus ON. Record callback max duration, p95/p99, missed deadlines, ASIO XRUN count, end-to-end vocal monitoring latency. Require no extra ASIO buffering and **no new ASIO XRUN** in reference test.
3. Unplug/restart VB-CABLE, exhaust secondary FIFO and force worker stall: overflows/underflows may increase, ASIO audio and timing must remain unaffected.
4. Verify USB interface ASIO monitor with GO LIVE OFF and ON; microphone+PC Music after all FX through `CABLE Output` in OBS/TikTok; no doubling/echo/loop.
5. If measurements fail, do not publish stable. Keep previous preview available for rollback.