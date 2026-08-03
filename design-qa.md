# SC220 Live design QA

final result: passed

## Comparison target

- Source visual truth: `C:\Users\me\AppData\Local\Temp\codex-clipboard-c5dbe738-7cca-4971-9a4a-3094ccd22945.png`
- Implementation screenshot: `C:\Users\me\.codex\visualizations\2026\08\02\019fc451-b04f-71e2-a2eb-87d22d994ebc\sc220-refined-pass2.png`
- Full-view comparison: `C:\Users\me\.codex\visualizations\2026\08\02\019fc451-b04f-71e2-a2eb-87d22d994ebc\sc220-design-comparison-pass2.png`
- Reference pixels: 1680 x 945.
- Implementation pixels and native viewport: 1600 x 900 at Windows DPI-aware 1x capture.
- Normalization: both surfaces are 16:9; the reference was downsampled to 1600 x 900 and placed next to the native 1600 x 900 implementation without cropping.
- State: the reference illustrates connected/active audio. The implementation is a real local runtime with SC220 unavailable, unsafe PC loopback blocked, and no programme audio. Dynamic status text, meter activity, and spectrum movement are therefore intentionally different.

## Findings

No actionable P0, P1, or P2 visual differences remain.

- Typography: Segoe UI Variable is applied consistently with small semibold headings, regular control text, tabular meter values, and no oversized display type.
- Spacing and layout: SC220, PC Audio, and Stream share one fader baseline and height. Input Trim and Auto Duck share one pre-fader row. Stream and Monitor occupy separate columns without collision.
- Colors and tokens: warm white faceplates, black title bar, restrained orange interaction accent, and labelled green/amber/red states preserve the approved direction without purple or decorative glow.
- Image quality: the original SC220 product photograph is used with a focused source crop; no substitute illustration is drawn. Its reduced opacity in the evidence is the intentional not-detected state.
- Controls: rotary controls use calibrated ticks, a layered metal rim, a recessed face, and a high-contrast pointer. Faders use a deep rail, brushed-metal cap, orange datum line, and aligned dB scale.
- Copy and content: the implementation adds real Monitor routing and anti-loop status from the current product branch. `VB-CABLE` is displayed as a compact alias while the real Windows endpoint name remains the routing authority.
- Icons: title-bar, Settings, enhancer power, and Monitor controls retain the repository's Lucide assets.
- Accessibility evidence: visible labels do not rely on color alone, control text remains high contrast, and the native JUCE controls preserve keyboard focus/interaction. Full assistive-technology compliance was not claimed or tested in this visual pass.

## Comparison history

### Pass 1

- Finding P2: each stereo meter rendered both L/R bars together on the left of its fader, making all three channel strips visually unbalanced against the approved hardware-mixer direction.
- Fix: expanded each meter across its channel strip, placed L and R bars at opposite edges, preserved the shared fader grid, and disabled meter mouse interception so the central faders remain directly operable.
- Evidence: `sc220-refined-final.png` before the fix and `sc220-refined-pass2.png` after the fix.

### Pass 2

- The SC220, PC Audio, and Stream meters symmetrically flank their faders.
- No overlap, clipping, broken text wrapping, or actionable P0/P1/P2 drift remains at 1600 x 900.
- Differences from the reference are expected product-state or functional-scope differences: selectable PC playback endpoint, independent physical Monitor bus, real routing-blocked state, and real no-signal spectrum state.

## Focused-region evidence

A separate crop was not needed. The comparison artifact preserves both complete 1600 x 900 surfaces at 1:1 pixel density in one 3200 x 940 image, so fader caps, meter placement, knob ticks, typography, selectors, and device imagery remain directly inspectable without density mismatch.

## Follow-up polish

- P3: validate the fully lit device photograph, active meters, moving spectrum, and non-empty loudness values with the physical SC220 and a safe PC playback endpoint.
- P3: repeat the visual capture at 1180 x 720 and maximised desktop size during hardware qualification.
