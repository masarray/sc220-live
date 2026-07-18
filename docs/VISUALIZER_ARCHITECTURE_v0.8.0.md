# Lightweight Liquid Visualizer — v0.8.0

## Authority

The architecture follows the current ArSonKuPik VST liquid visualizer at the
commit recorded in `ASKP_AUTHORITY.lock`:

- visual work runs on the message thread;
- the audio thread only writes a lock-free stereo tap;
- no kick classifier, tempo detector or beat prediction;
- fixed liquid and particle pools;
- liquid silhouette is the hero animation.

## Thread contract

```text
Audio callback
  -> VisualScopeTap::push
  -> bounded memcpy into 16,384-sample SPSC queue
  -> returns immediately; excess visual data is discarded

Message thread at 24 Hz, only while the window is visible
  -> consume newest <= 4096 samples
  -> six-band time-domain filter-bank analysis
  -> fixed liquid/particle update
  -> repaint visualizer bounds only
```

The queue has no mutex, allocation, retry loop or back-pressure. It is not an
audio transport. A dropped visual update is harmless and is counted separately
from audio dropouts.

## Fixed visual budget

- 4096 newest samples analysed per visible refresh;
- six broad musical bands;
- 32 spring/fluid nodes;
- eight bubbles;
- twelve droplets;
- one cached back layer;
- one cached front/shade layer;
- one cached top-gloss layer.

There is no visual FFT, dynamic particle container, glitter field, tempo model or
beat prediction. Musical Noise Clean has a separate fixed transform because its
audio algorithm genuinely requires spectral processing.

## Cached layers

`LiveLiquidVisualizer::rebuildStaticLayers()` runs only after resize or bypass
appearance changes. It caches the outer metal/glass rim, dark vessel, side/bottom
shade, cropped sphere gloss and front edge. Per-frame painting is limited to the
smooth liquid surface and active fixed particles.

## Visual semantics

- sub/low bands: broad displacement and slosh;
- body bands: smaller secondary movement;
- upper bands: fine local ripple and occasional micro-bubble;
- stereo energy difference: impact position and slosh direction;
- bypass: desaturated vessel;
- silence: the spring lattice settles, repaints stop, and tray mode disables both
  the UI timer and tap producer.
