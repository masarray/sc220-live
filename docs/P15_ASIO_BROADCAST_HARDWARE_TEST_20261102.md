# P15 — Hardware-test candidate / realtime digital console (2026-11-02)

## Milestone outcome
As with professional live mixers, the performer's **SC220 ASIO headphone bus is not the audience Broadcast bus**. The program post-FX/post-limiter mix goes directly into the interface's native ASIO output and is then copied into an independent SPSC worker for Windows VB-CABLE. Windows WASAPI is a secondary delivery mechanism, never a timing master.

## Important fixes implemented together
1. Remove the old startup code that inspected JUCE output devices and automatically selected VB-CABLE as the *primary* device. Device selection is now explicit and independent: use Audio Setup for SC220 ASIO, the BROADCAST selector for CABLE Input.
2. Remap the UI STREAM fader to `broadcastOutput.setGainDb`, independent of native ASIO master gain. Existing settings' legacy `outputTrimDb` is migrated to `broadcastGainDb`, then native master trim is restored to 0 dB on the new first run. Saved broadcast gains load separately after migration.
3. Display dual-clock health in the small footer: ASIO estimated latency, callback CPU load, callback deadline misses, and BC OFF/WAIT/ERROR/LIVE plus broadcast overflows. These are observed callback deadlines and secondary FIFO metrics, **not** measured electrical loopback latency or verified ASIO-driver XRUN counts.
4. Add a test target exercising the real MonitorOutputClient's physical-only vs cable-only endpoint policies and offline source-clock preparation; run this test with CTest. The source guard additionally checks independent stream fader, no startup hijack, and post-master ASIO/broadcast routing.

## Operator's 15-minute acceptance
1. **Extract the complete P15 ZIP.** Keep the `Plugins` folder beside `SC220 Live.exe`. Start with hardware headphone/monitor volume low.
2. Settings → Audio Setup → choose the native SC220 ASIO driver for IN/OUT, 48 kHz, 256 samples first. Use SC220 headphones, not Windows Listen to This Device. Sing or speak: confirm effects are audible in headphones with **GO LIVE OFF**.
3. In the mixer choose BROADCAST → `CABLE Input (VB-Audio Virtual Cable)`. In OBS/TikTok choose `CABLE Output` as an audio input. Disable duplicate microphone/desktop captures in OBS. Turn GO LIVE on. Confirm the **post-FX** voice/music appears in OBS and the native ASIO headphone monitor does not change in level.
4. Move STREAM fader to -30 and +0 dB while monitoring through SC220: audience volume changes, performer headphones do not. Press GO LIVE OFF: OBS goes silent, performer continues hearing ASIO post-FX.
5. Unplug or disable VB-CABLE in Windows, or close OBS: native ASIO must continue. Check footer BC WAIT/ERROR and BC DROP counters; reconnect and reselect CABLE Input if necessary. Do not use a broadcast loopback device as the PC MUSIC source.
6. Repeat 48 kHz at 128 and 64 samples only if the hardware/CPU supports them. Compare callback CPU, missed deadlines and headphone stability with Broadcast OFF vs ON. Require zero newly introduced ASIO deadline misses during A/B and no change to driver buffer size. Observe for at least 30 minutes; record actual crackles, buffer sizes and OS details.

## Safety / limitations
The code adds no ASIO buffer stage for WASAPI and does not wait for its worker; a preallocated FIFO copy still has small CPU cost. The existing callback deadline meter is not a vendor-specific hardware XRUN counter. Real PC/interface tests remain required. This is a prerelease for hardware evaluation, not stable. Physical WASAPI AUX monitor is entirely optional and must never substitute for the singer's direct ASIO feedback.

## Reference practices
- JUCE AudioIODeviceCallback: https://docs.juce.com/master/classjuce_1_1AudioIODeviceCallback.html
- JUCE Windows native ASIO callback: https://github.com/juce-framework/JUCE/blob/master/modules/juce_audio_devices/native/juce_ASIO_windows.cpp
- Microsoft WASAPI shared render and stream switch: https://learn.microsoft.com/en-us/windows/win32/coreaudio/rendersharedeventdriven
- Microsoft WASAPI: https://learn.microsoft.com/en-us/windows/win32/coreaudio/wasapi
- OBS virtual audio cable: https://obsproject.com/kb/application-audio-capture-guide
