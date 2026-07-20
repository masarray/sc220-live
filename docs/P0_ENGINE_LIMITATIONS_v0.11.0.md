# P0 Engine Scope and Limitations

v0.11.0 deliberately avoids claiming a custom cross-device clock-drift resampler.

The current Windows transport still uses JUCE's active audio-device backend. The Stability Guard adds:

- native/estimated xrun telemetry;
- deadline and callback-gap detection;
- conservative automatic buffer fallback;
- cross-endpoint safe-floor policy.

A future native backend may separate WASAPI capture and render clients behind an SPSC elastic bridge with fractional drift correction. That remains a distinct architecture milestone because it changes the transport layer, adds measurable latency, and requires long hardware soak testing.
