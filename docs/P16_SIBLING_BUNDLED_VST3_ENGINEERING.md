# P16 — ArSonEQ / ArSonComp / ArSonVerb as bundled SC220 VST3 inserts

## Runtime behavior
Three **independently compiled VST3 bundles** ship in the portable `Plugins/` folder.
Voice FX1 automatically loads ArSonEQ (M3.1 engineering build), Voice FX2
loads ArSonComp v0.2.0, and Voice FX3 loads ArSonVerb v1.0.1.
The existing ArSonKuPik Signature engine still operates as a separate built-in
voice/music processor, between FX1 and FX2 on Voice. Therefore the actual
voice signal path is: EQ -> ArSonKuPik Signature -> Comp -> Verb.
This is intentionally not advertised as three contiguous insert positions.

Music FX1/FX2/FX3 stay EMPTY at startup to avoid unnecessarily running another
three plugin instances. Click an empty FX name to browse your own VST3; click
the `...` menu on ANY Voice or Music insert to load a verified built-in EQ,
Comp, or Reverb directly. Filled FX name opens its own editor; ON/OFF bypasses
only that slot and does not reconfigure ASIO. No runtime downloads or remote
scanning ever occur.

## Verified release authority
- ArSonEQ source: https://github.com/masarray/arson-eq/tree/1f9513d75faea306e06bafd7b68119d4b8bef276
  Binary release: https://github.com/masarray/arson-eq/releases/tag/v0.1.0-m3.1-sc220-test
  This is M3.1 engineering binary; it is **not yet a stable ArSonEQ release**.
  It uses bundled HTML/JS GUI in a native Windows JUCE WebView2 editor; opening
  the UI requires Windows WebView2 runtime, not a browser/network connection.
- ArSonComp source: https://github.com/masarray/arson-comp/tree/0a039ef6d340e26aa01a62df3670f74da1f03a12
  Binary VST3-only release: https://github.com/masarray/arson-comp/releases/tag/v0.2.0-sc220-vst3-test
  Its Windows build commit differs from the stable v0.2.0 source tag by docs;
  checked actual CMake, engine, plugin and artwork tree SHAs are identical.
- ArSonVerb stable VST3: https://github.com/masarray/arsonverb/releases/tag/v1.0.1
- Pinning/asset digests: `scripts/sibling-vst3-manifest.json`.

## Packaging and performance policy
One CI build compiles SC220 and the existing 4 CTest suites; the completed
VST3 binaries are downloaded independently from the sibling product release
assets. Every ZIP has a pinned, reviewed SHA-256 and safe ZIP staging policy.
The same native SC220 host runs the three plugins through a generic stereo
VST3/finite-output smoke test. Any missing release or hash mismatch FAILS
the package; never substitute a mutable latest version or silent empty slot.
The outer test ZIP is uploaded as a 7-day Actions artifact. SC220 does not
clone/build all sibling repos or invoke GitHub from the audio callback.

Do not enable three extra PC MUSIC inserts by default; doing so would
unnecessarily increase realtime CPU/VST processing. Actual loaded plugin
processing latency is plugin-dependent and remains subject to ASIO A/B tests
at 48 kHz, 256/128/64 buffer sizes.

## Licence and release policy
All three sibling plugins are AGPL-3.0 and SC220 Live is currently labelled
proprietary. This is a distinct Windows hardware integration preview; it does
not automatically resolve licensing obligations that may arise from distributing
the host with the plugin binaries. Keep associated corresponding source links,
licences and build provenance available, and resolve combined-work/dual licence
status before stable/commercial redistribution. See THIRD_PARTY_NOTICES.md.

## Host compatibility refinement (sidechain FX)
ArSonComp declares a main stereo I/O plus an optional stereo sidechain input.
A one-input-bus `BusesLayout` is structurally invalid for this plugin, even
though its default main chain supports stereo. The SC220 host now starts from
the plugin's advertised layout, forces main stereo, explicitly disables all
optional sidechain/auxiliary buses, and still strictly validates exactly
two ACTIVE input channels plus two output channels. This is not a mono
fallback and does not mix external sidechains into PC Music/ASIO. The Windows
generic host smoke checks the actual ArSonComp VST3 with this contract.
