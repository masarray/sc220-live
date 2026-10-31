# SC220 Live Real-Time Engine — P0 Hardening Contract

**Source authority:** `live/LiveAudioEngine.cpp` plus the CMake-generated
`LiveAudioEngine_Monitor.cpp`, and `MonitorOutputClient.cpp`. Do not audit
raw engine sources alone: `qa/verify_effective_cmake_pipeline.cmake` runs the
actual deterministic UI/engine/application generator chain before compilation.

## Zero added host buffering, not zero physical latency

- JUCE Windows builds enable WASAPI and ASIO (`JUCE_ASIO=1`); users select
  their installed driver in Audio Setup. Do **not** force an ASIO device switch:
  input/output must remain a compatible pair under the selected driver.
- A fixed 1 ms ASK-P safety lookahead exists, and the selected VST3 effect may
  report additional latency. The physical interface, stream buffer, and monitor
  output contribute further latency. Do not label the sum as 0 ms.
- Static-noise LEARN now analyses a separate preallocated FFT workspace but
  **never modifies the live voice buffer**; its FFT delay must not reach audio.
- Main broadcast latency excludes separate physical WASAPI monitor buffering.
  Use measured input-to-output impulse latency for the actual microphone path.

## Single owner per real-time domain

```text
             UI/Settings                   VST3 main-thread host
                 |                          scanner/prepare/editor
             atomics                     prepared candidate
                 |                              |
ASIO/WASAPI -> LiveAudioEngine callback <- brief quiescent install/retire
                 |
         bounded DSP, meters, protected master
                 |                         |
           stream output            SPSC float FIFO (drop newest on full)
                                           |
                                   monitor worker (COM, resample,
                                   drift, render, reconnect)
                                           |
                                     physical monitor
```

1. Callback **must not** open, close, reset, or reconnect WASAPI devices. The
   monitor worker owns COM and its driver APIs. Callback only enqueues data to a
   preallocated SPSC FIFO, with a non-blocking publication gate.
2. Only the callback writes the monitor FIFO; only the worker reads. Neither
   side mutates the other's read/write index when full; overflow drops a block
   and increments the counter.
3. Source/device changes gate new publications and drain in-flight writers
   from a **non-audio** thread before transport reset. No callbacks wait.
4. VST3 scan, instantiation, editor, prepare, release and destructor belong on
   the main thread under the VST3 lifecycle contract. A new trusted stereo VST
   instance is fully prepared before a brief callback quiescence gate swaps it.
   Plugin replacement no longer calls remove/addAudioCallback, which previously
   restarted the entire native enhancer and PC music capture.
5. VST3 bypass uses a 12 ms fade between wet and latency-aligned dry with
   preallocated scratch; the VST3 stays warm while bypassed. Plugin replacement
   still requires real-world click/tail validation; it is **not** an isolated
   crash-safe or click-free arbitrary-plugin rack.
6. The ASK-P v0.5.24 authority uses one canonical prepared engine with bounded
   coefficient retuning, not the obsolete triple-engine builder described in
   older documentation. Do not claim background DSP rebuild without verifying
   the pinned submodule and generated effective DSP.
7. The P6B hiss suppressor states belong to each `LiveAudioEngine` and reset
   with device preparation. No function-static shared audio histories.

## Real-time callback prohibitions

- No heap allocation, file or network operations, GUI calls, device scans,
  VST3 loading, mutex acquisition, sleeps or potentially blocking COM calls.
- Bound CPU per block, use preallocated output and analyzer buffers, stable
  ownership, atomic control snapshots, sensible coefficient coalescing, and
  vectorised buffer operations when beneficial.
- A third-party plugin can still allocate/lock internally; caller must not
  assume a generic VST3 is real-time safe. Only trusted effects until crash
  sandboxing and per-plugin deadline enforcement exist.

## Physical acceptance: measured, not asserted

| Gate | Acceptance condition |
| --- | --- |
| Build | x64 Windows Release build with pinned ASK-P submodule and JUCE ASIO |
| ASIO | Select actual hardware full-duplex driver; verify no silent fallback |
| Low latency | Measure round-trip impulse at 48 kHz / 256, 128, 64 samples |
| No-plugin | 8 h continuous monitoring: no callback deadline misses and no XRUN growth |
| ArSonVerb | Repeated load, editor, bypass, reload, sample rate and buffer changes |
| Transitions | 500 bypass/preset operations; no audible click or invalid output |
| Lifecycle | 500 load/remove cycles; no leaks, crash, use-after-free or deadlock |
| Reconnect | Physical unplug/replug, stream endpoint change, sleep/resume |
| Telemetry | Record callbackPeakPercent, callbackDeadlineMisses, PC underflows and monitor underflows |
| Faults | Null/NaN/Inf test, sustained CPU load, hardware-driver recovery |

Telemetry is not proof of end-to-end latency: `callbackDeadlineMisses`
counts observed callback duration greater than buffer period, not OS scheduler
dispatch lateness. External impulse measurement and actual audio hardware are
required.

## CI and preview safety

- Cheap GitHub-hosted source guard validates the exact effective generator chain
  with no private DSP secrets and never uses a self-hosted runner.
- A separate `sc220-download` manual Windows preview workflow can compile the
  branch into a five-day unsigned EXE artifact once its **repository secret**
  `SC220_SOURCE_TOKEN` is configured with private ASK-P read access.
- Until a real Windows compile and hardware pass exist, keep this change in PR
  as a development milestone, not a public stable release.

Industry guidelines: PortAudio callback tips,
https://github.com/PortAudio/portaudio/wiki/Tips_Callbacks
and the VST3 threading model,
https://steinbergmedia.github.io/vst3_dev_portal/pages/Technical%2BDocumentation/API%2BDocumentation/Index.html.
