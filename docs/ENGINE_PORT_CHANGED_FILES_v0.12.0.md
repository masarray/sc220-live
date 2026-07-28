# v0.12.0 Engine Port — Changed Files

## New

- `live/EnhancerEngineBridge.h/.cpp`
- `live/SonkupikSphereVisualizer.h/.cpp`
- `third_party/askp-engine-v0.5.19/`
- `third_party/sonkupik-sphere-v0.10.4/`
- `docs/ENHANCER_ENGINE_PORT_v0.12.0.md`
- `qa/audit_v0120_engine_port.py`

## Refactored

- `live/LiveAudioEngine.h/.cpp`
- `live/LiveMainComponent.h`
- `CMakeLists.txt`
- Windows build/package scripts

## Removed dependency

- neighbouring `askp-vst` checkout;
- configure-time visualizer source patching;
- automatic authority sync during normal builds.
