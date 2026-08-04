# SC220 Live design QA

final result: passed

## Visual authority and evidence

- Approved source: `C:\Users\me\AppData\Local\Temp\codex-clipboard-b0301b08-3e2f-451a-97e8-689f2f461ff2.png`
- Release-build capture: `C:\Users\me\.codex\visualizations\2026\08\02\019fc451-b04f-71e2-a2eb-87d22d994ebc\sc220-fidelity-final.png`
- Focused 1:1 comparison: `C:\Users\me\.codex\visualizations\2026\08\02\019fc451-b04f-71e2-a2eb-87d22d994ebc\sc220-control-comparison-final.png`
- Both full surfaces were normalized to 1600 x 900 before equal-size crops were
  placed on one comparison canvas. Neither control crop was stretched.

## Resolved P1 fidelity findings

### Rotary controls

The earlier build used line ticks, a heavy layered surround, and a nearly full
value arc. The final control now matches the approved hardware grammar:

- 21 small dot calibration marks with restrained major dots;
- thin satin-metal bezel and dominant recessed black face;
- subtle four-facet machining sheen and crisp white pointer;
- short fixed orange signature accent on the right;
- macro values above the knob and readable `0% / 100%` endpoint labels below.

### Linear faders

The earlier 48 x 38 cap was too wide and squat. The final fader uses a 40 x 68
vertical cap with dark side cheeks, brushed aluminium face, fine machining
lines, dark centre datum, top highlight, deep black rail, and aligned dB scale.
SC220, PC Audio, and Stream share the same control renderer.

## Final comparison findings

No actionable P0, P1, or P2 differences remain for the knob and fader fidelity
scope requested by the user.

- Typography remains Segoe UI Variable with calm hierarchy; macro labels and dB
  scales were increased after the final comparison for practical readability.
- The warm-white surface, black title bar, orange signature accent, restrained
  borders, and physical-control material treatment remain consistent.
- The real SC220 product photograph and repository Lucide icons are unchanged.
- Native JUCE sliders remain functional, keyboard reachable, and mapped to the
  existing DSP/audio-engine parameters; no visual-only fake controls were added.
- The evidence runtime correctly shows the local no-device/no-signal state.
  Active meters, spectrum movement, loudness values, and detected-device opacity
  are intentionally different from the connected-state proposal.

## Remaining qualification

- P3: repeat the visual capture with the physical SC220, a safe PC playback
  endpoint, active programme audio, and a selected physical monitor output.
- P3: verify the compact surface at the repository's smaller supported viewport
  during hardware qualification.
