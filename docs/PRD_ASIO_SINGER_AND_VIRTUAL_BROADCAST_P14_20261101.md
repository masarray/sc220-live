# PRD — P14 SC220 Live Realtime Singer Monitor and Best-Effort Broadcast

## User outcome
A singer plugs the microphone/headphones into a real SC220/ASIO interface, opens the low-latency ASIO engine, sings with Voice VST3/FX enabled and hears their **own fully processed voice immediately through the interface's ASIO output**. Viewers simultaneously receive the same mastered stereo programme through OBS/TikTok Live Studio using VB-CABLE, with independent (potentially higher) broadcast delivery latency.

## P0 requirements (non-negotiable)
| ID | Requirement | Acceptance |
|---|---|---|
| R1 | Single physical ASIO device input/output | ASIO input and output remain same native driver, no secondary device coercion |
| R2 | Singer monitor never depends on GO LIVE | ASIO headphone mix audible GO LIVE OFF, ON, or with VB-CABLE absent |
| R3 | Broadcast of completed FX output | Same post-FX/post-master limiter stereo frames appear at OBS `CABLE Output` |
| R4 | Broadcast failure cannot stall ASIO | WASAPI worker stopped/unplugged/overloaded => bounded drops only, no blocking |
| R5 | Separate output routing | Primary ASIO in Audio Setup; Broadcast virtual render in main Output; optional physical Aux Monitor secondary |
| R6 | Consistent clocking | ASIO is master; WASAPI worker handles native mix format and drift |
| R7 | Feedback prevention | PC Music capture must never select own broadcast CABLE output |
| R8 | Free public compatibility | No payment pop-up or trial expiry for SC220 application; unaffected independent VST licences |
| R9 | Realtime measurable | Per-run deadline, dropout, FIFO under/overflow metrics exposed via existing Status/debug |
| R10 | No blind rollout | Real ASIO 48 kHz 256/128/64 A/B hardware proof before merge to stable |

## Operator workflow
1. Open Audio Setup: choose native SC220 ASIO driver, input 1/2, output 1/2, sample rate 48 kHz and appropriate block size. Headphones physically connected to ASIO interface.
2. Choose Voice/Music FX and adjust Master. The native ASIO output remains alive at all times, even with streaming disabled.
3. Choose BROADCAST / VB-CABLE = `CABLE Input (VB-Audio Virtual Cable)`; select optional independent physical AUX MONITOR only if needed.
4. In OBS add Audio Input Capture = `CABLE Output`; in TikTok LIVE Studio set microphone/input = `CABLE Output`. Avoid capturing direct mic and virtual cable twice.
5. Press GO LIVE to enable copy towards viewer. Press again to stop ONLY broadcast, **not** the performer headphone monitoring.
6. If virtual endpoint is removed or Windows audio engine fails, show broadcast degraded status but keep ASIO running; allow reconnect without restarting ASIO.

## Non-goals
- No attempt to make WASAPI output as low-latency as native ASIO.
- No multi-ASIO aggregate device, ASIO4ALL chain, Windows “Listen to this device” monitoring, kernel driver or injection.
- No mastering DSP duplication, per-plugin ASIO buffering, or automatic primary output switch to VB-CABLE.
- No promise of absolute 0 ms round trip for any hardware.

## Milestone definition of done
- ARD + PRD checked into repo alongside engine change and a PR, generated code compiles and tests pass.
- User tests on native SC220 ASIO with recorded A/B results; no new ASIO deadline misses.
- VB-CABLE route and OBS proof; 60-minute live run; worker-stall/failure drills; stable only after evidence.
- Reuse current `MonitorOutputClient` transport with a virtual-only policy and a separate instance. No fragile patching of the core DSP callback in multiple places.

## References
ARD contains primary JUCE, Microsoft, OBS and VB-Audio documentation URLs.