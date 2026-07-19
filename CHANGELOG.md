# Changelog

## v0.9.9 - 2026-07-19 - P0 premium workspace redesign

- Added one authoritative `WorkspaceLayout` shared by `paint()` and `resized()`.
- Added a central spacing/radius metric system in `LiveTheme.h`.
- Reworked the header into brand, title, engine-status capsule and quiet utility controls.
- Cached Lucide active/inactive icons and added lightweight fallback glyphs.
- Added a unified premium Input/Output routing strip.
- Rebalanced Musical Noise Clean into controls, compact expander monitor and sphere zones.
- Rebuilt the expander monitor with a professional 0 to -24 dB reduction convention.
- Added a clear PRESET hierarchy and equal five-column macro grid in Smart Sound Enhancer.
- Refined card depth, header separators, border precision and top highlights.
- Added Option A P0 implementation audit and Option B layout blueprint documentation.

## v0.9.7 - 2026-07-19 - Headless processor linker fix

- Fixed `LNK2001` unresolved `askp::QrisLicensePanel` constructor/destructor.
- Sonkupik Live no longer compiles the askp-vst `PluginEditor.cpp` or `UpdateChecker.cpp`.
- Generates a headless `PluginProcessor.h` with `hasEditor() == false`.
- Generates headless reference/live processor translation units with `createEditor() == nullptr`.
- Keeps authoritative DSP, all factory presets, LicenseManager, ScopeTap, OrbVisualizer, limiter and gain policy.
- P2 renderers now use headless processor variants; editor removal cannot affect rendered audio.
- Removes VST-only QRIS/activation-editor dependency from the Live executable.

## v0.9.6 - 2026-07-19 - P2 preset-by-preset audio parity

- Added separate authoritative and Live offline renderers for all 40 factory presets.
- Added bit-exact FNV-1a output fingerprints plus RMS, peak, latency and sample-count comparison.
- Added `scripts/run-p2-parity.ps1` and optional CMake parity targets.
- Added a Settings action that applies clear VST reference conditions for manual A/B tests.
- Preserved the P1 12 ms control-response overlay as the only processor-source delta.

## v0.9.2 - 2026-07-19 - P1 live knob response

- Audited the complete UI-to-DSP parameter path for Enhance, Smart Bass, Smart Treble, Vocal Body, and Stereo Magic.
- Identified the authoritative VST's 80 ms `SmoothedValue` glide as the remaining source of the detached live-control feel during continuous dragging.
- Added a configure-time generated `PluginProcessor_Live.cpp` overlay that changes only creative-control smoothing from 80 ms to 12 ms.
- Kept the authoritative DSP engine, 40 presets, 400 Hz coefficient-retune ceiling, preset transition, WOW calibration, bypass, lookahead limiter, output ceiling, and settled parameter values unchanged.
- Added a hard CMake anchor check: build stops instead of applying the overlay to an unaudited `askp-vst` processor revision.
- Added static and mathematical response audits for common 48 kHz buffer sizes.

## v0.9.0 - 2026-07-19 - Full VST parity and functional rack controls

- Replaced the custom Live enhancer wrapper with the authoritative `askp::Processor` from `askp-vst`.
- Restored exact VST macro smoothing, 400 Hz in-place retune, dual-engine preset handover, WOW calibration, one-millisecond lookahead safety, meters, and latency-aligned bypass.
- Removed the delayed 35 Hz background full-engine rebuild responsible for sluggish knob response.
- Replaced the shortened Live liquid implementation with the exact VST `OrbVisualizer` and scope tap.
- Added a visible Smart Enhancer bypass button.
- Wired Input Trim, Noise Clean, and Output Trim rack power controls.
- Replaced misleading disabled external-slot power icons with functional ADD actions.
- Preserved the locked final VST safety limiter.
- Added a parity and interaction root-cause audit.


## v0.8.1 - 2026-07-19 - Windows compile fix and leaner JUCE linkage

- Removed the stale `displayProfile` reset loop left behind when the old spectrum atomics were deleted; this fixes MSVC errors C2065/C2530/C3531/C2143 in `MusicalNoiseClean.cpp`.
- Replaced deprecated JUCE `Display::userArea` with `userBounds`.
- Kept the intentional cache-line separation in `VisualScopeTap` and locally suppressed MSVC C4324 for that type only.
- Removed the unused `juce_audio_utils` target dependency, preventing its transitive `juce_audio_processors` compilation in the Live Safe application.
- Added a regression audit for the exact Windows compiler failure.

## v0.8.0 - 2026-07-18 - Authoritative VST sync and lightweight liquid engine

- Locked Sonkupik Live to `masarray/askp-vst` v0.5.14 commit `655874dee6ee69df7212b8073ab941f9607d8faa`.
- CMake now compiles the exact current Smart Enhancer DSP and all 40 factory presets from the private authority checkout instead of the stale local recipe.
- Fixed an authority-shadowing bug by changing `LiveAudioEngine.h` to include `arsonkupik_dsp.hpp` from the selected CMake include path.
- Added automatic authority discovery and authenticated locked-source sync to `.deps/askp-vst`.
- Replaced the centre spectrum display with the current ArSonKuPik liquid-sphere concept: 4096 newest samples, six time-domain bands, 32 fluid nodes, eight bubbles and twelve droplets.
- Added a current-style 16,384-sample SPSC visual tap; visual data drops instead of delaying audio.
- Cached vessel/glass/gloss layers, added settled-frame sleep and fully disabled the visual tap/timer in tray mode.
- Removed the legacy GUI spectrum class and per-frame display atomics.
- Replaced the denoiser's JUCE FFT dependency with a preplanned fixed 256/512 radix-2 transform to avoid shared FFT lock/dispatch overhead.
- Removed unused `juce_dsp`, `juce_audio_processors` and dormant VST-host linkage from the default Live Safe target.
- Adopted the current VST runtime contract: numeric preset index, preset Smart Protect, external bypass and external peak-limiter ownership, configure-then-reset engine initialisation.
- Coalesced expensive full-engine background rebuilds to roughly 35 Hz during macro drag.
- Removed one redundant full-buffer copy and fused level-meter scanning into one pass.
- Added average and callback-peak deadline telemetry.
- Added authority, visualizer, sync and realtime audit documentation plus a 44/44 source audit.

## v0.7.0 - 2026-07-11 - Sonkupik Live workspace redesign

- Renamed the product-facing desktop application to **Sonkupik Live** while preserving the ArSonKuPik Smart Enhancer/VST identity.
- Removed runtime Unicode decoration and enabled MSVC `/utf-8`, eliminating status/menu mojibake.
- Replaced the old setup-form layout with a signal-flow layout: Before Enhancer rack and input fader on the left, Musical Noise Clean + Smart Enhancer in the centre, After Chain rack and output fader on the right.
- Kept only INPUT and OUTPUT device selectors on the main workspace. Sample rate, buffer size, channel mode and denoise quality now live in an integrated settings CallOutBox.
- Rebuilt the spectrum from the ArSonKuPik VST analysis strategy: logarithmic 80 Hz-16 kHz bands, band peaks, pink-noise visual calibration, fast attack/slow release and spatial smoothing.
- Standardised all eight main knobs to one compact premium geometry.
- Fixed knob numeric readouts by repainting the parent control through a persistent Slider listener while dragging.
- Connected the side faders to real input/output trim parameters and persisted their values.
- Added output trim processing before the locked final safety limiter.
- Added `docs/LAYOUT_AUDIT_v0.7.0.md` and refreshed the UX reference/specification.

## v0.6.2 - 2026-07-11 - Visual Studio 2026 compile compatibility

- Fixed MSVC C3535/C2440 in `LiveApplication.cpp` caused by heterogeneous braced pointer lists with `auto*`.
- Replaced mixed derived-component initializer lists with an explicit `std::initializer_list<juce::Component*>`.
- Applied the same compiler-safe pattern to the main control registration and rack row layout.
- Added a source audit that rejects heterogeneous `auto*` component lists in the live application.

## v0.6.1 - 2026-07-11 - Visual Studio 2026 Build Detection
- Replaced the hard-coded `Visual Studio 17 2022` generator in the live Windows build helper.
- Added Visual Studio discovery through `vswhere` with an explicit check for the Desktop development with C++ workload.
- Added automatic selection of the newest CMake, preferring the CMake bundled with Visual Studio.
- Added native `Visual Studio 18 2026` generation when CMake 4.2+ is available.
- Added automatic Ninja + MSVC v145 fallback when the installed CMake is too old for the VS 2026 generator.
- Added stale CMake generator-cache detection and automatic build-tree cleanup.
- Added explicit VS 2026, Ninja/MSVC and legacy VS 2022 CMake presets and updated Windows build documentation.

## v0.6.0 - 2026-07-11 - ArSonKuPik Live Source Milestone
- Added a dedicated native C++/JUCE `ArSonKuPikLive` desktop target while preserving the existing VST3 target.
- Reused the same Smart Enhancer DSP and factory preset sources as the VST.
- Added interface-input to VB-CABLE-output audio routing through JUCE AudioDeviceManager.
- Added learned short-FFT Musical Noise Clean with stereo-linked conservative spectral attenuation, transient/tonal protection, Live 256 and Clean 512 modes.
- Added a smooth learned-noise-relative downward expander and locked final linked safety limiter.
- Added premium compact UX using embedded Plus Jakarta Sans and Lucide SVG icons, including spectrum/profile display, meters, rack rows and status footer.
- Added close-to-tray behaviour, tray actions and a compact controller.
- Added Windows build/package scripts and a local JUCE source override for unreliable/offline networks.
- Added comprehensive product, architecture, DSP, UX, real-time safety, test and VST3-hosting roadmap documentation under `docs/`.
- External VST slots are intentionally reserved but disabled until crash-isolated scanning, latency reporting, state recall and safe bypass are complete.
- Added gentle 24 Hz two-stage subsonic cleanup ahead of the learned spectral stage.
- Added a latency-aligned dry path so denoise bypass and Hear Removed do not create timing/comb artefacts.
- Moved Smart Enhancer preset/macro rebuilds off the audio callback into a background triple-engine builder with a 5 ms crossfade.
- Made FFT quality changes commit through audio restart and invalidate incompatible learned profiles.
- Expanded settings persistence to include preset, channel mode, macros, denoise/expander controls and engine state.
- Added source-level real-time, structure, documentation-link and syntax audit coverage under `qa/`.
- Fixed rapid enhancer bypass during an active engine crossfade so no prepared-engine slot can remain stranded.
- Separated output mute from engine power; muting now silences only the final device copy while DSP and meters continue running.

## v0.5.2 - 2026-07-11 - Clean Licensing UI, Embedded Font, Smooth Preset Switching
- Removed all non-ASCII separators from user-facing licensing text to prevent mojibake on Windows builds with non-UTF-8 compiler settings.
- Embedded Plus Jakarta Sans Regular and SemiBold directly in BinaryData; the plugin no longer depends on the font being installed on the user's computer.
- Reworked preset changes into a staged fade-out / zero-gain topology commit / fade-in transaction. New preset macros and Output Trim can no longer leak into the old preset before the fade begins.
- Reset the WOW compensation state when the new preset is committed, preventing stale loudness compensation from creating a transition bump.
- Renamed the visible protection strip from PROTECT to SMART PROTECT.
- Kept beta remaining days inside the activation/status card opened by double-clicking MASARRAY AUDIO.

## v0.5.0 — 2026-07-10 — DAW Context Presets & SonKu Deep Chest
Factory presets are now designed around where the plugin is inserted in a DAW, not only around tonal colour.
- **MasAri renamed to Mas Ari Signature** and remains the default reference with the original luxurious, lively, 3D and ear-tickle direction plus a controlled +4 dB WOW target.
- **26 focused presets in five compact popup sections:** 1 Signature, 8 Master, 6 Mix Bus, 6 Track and 5 Creative presets. The ComboBox uses section headings without adding bulky cards or extra panels.
- **Context policy added to every preset:** target WOW lift, pre-chain peak target, maximum smart-makeup restoration, limiter-GR budget and maximum stereo width. The wrapper now reads each preset's target instead of forcing Master, Mix Bus and Track contexts to the same +4 dB.
- **Master contexts:** Clean, Modern, Warm, Open 3D, Low-End Control, Vocal Focus, Streaming Safe and Dynamic.
- **Mix Bus contexts:** Glue, Punch, Depth, Air, Warmth and Clean-Up, targeted around +1 dB so they preserve downstream mastering headroom.
- **Track contexts:** Vocal Polish, Drum Punch, Bass Tight, Guitar / Synth Width, Acoustic Natural and Podcast, targeted close to unity (+0.5 dB nominal).
- **Horeg/Battle/Balap consolidated:** all three legacy IDs are removed and replaced by **SonKu Deep Chest**. It combines controlled 58 Hz depth, 74 Hz torque and 112 Hz chest impact, with extra headroom and mono-bass stability. The old dark top end is corrected with 4.2 kHz sparkle, 7.2 kHz detail and a +2.35 dB shelf at 12.8 kHz.
- **Context-aware engine recovery:** clean post-rack headroom restoration is stronger for Mix Bus/Track (to remain near unity) and controlled for Signature/Creative. Limiter-GR budgets feed the makeup back-off logic.
- **Context width caps:** macro Stereo Magic cannot exceed each preset's intended maximum width; Mix Bus and low-end-focused presets remain conservative.
- **Preset parameter version bumped to 2** because the factory choice list was deliberately rebuilt and reordered.
- **QA:** `preset_context_audit`, `prechain_headroom_audit` and context-aware `smart_chain_headroom_audit` all PASS. All 26 presets remain finite; full-chain peak is approximately 0.89 in moderate and hot dense-program tests.

## v0.4.2 — 2026-07-10 — Clean Pre-Chain Headroom, Stable WOW, No Clip Distortion
Perbaikan difokuskan pada keluhan audio masih pecah/hancur karena sinyal memasuki creative rack terlalu hot sebelum processing.
- **Smart pre-chain guard baru:** input peak ditangkap instan sebelum EQ/compressor/exciter/width. Target working peak adaptif sekitar **−10 sampai −14.3 dBFS** mengikuti agresivitas Enhance/Bass/Treble/Vocal/Stereo. Attenuation turun instan, recovery 650 ms agar tidak pumping.
- **Parallel compressor kembali unity:** equal-power blend lama menambah hampir +3 dB pada dry/wet yang berkorelasi. Diganti linear parallel blend sehingga compressor tidak membuat hidden gain sebelum color stage.
- **Smart makeup tidak lagi menggandakan loudness macro:** makeup kini terutama mengembalikan headroom yang berguna, dibatasi risk, pre-limiter peak, dan limiter GR. WOW +4 dB tetap menjadi tugas wrapper, bukan dipaksa dari dalam rack.
- **Limiter engine tanpa tanh clipper:** linked sample-peak attack instan + release halus. Transient sekarang diturunkan dengan gain, bukan dibentuk ulang oleh soft clip nonlinear.
- **WOW measurement stabil:** dry/wet memakai power envelope sekitar 850 ms, bukan rasio RMS per block. Ini menghilangkan stale attenuation dan pumping setelah smart headroom bergerak. Freeze preset sekarang berbasis sample/time, bukan jumlah block.
- **Hard headroom cap nyata:** gain WOW yang benar-benar diaplikasikan selalu dibatasi peak block saat ini; koreksi servo dibatasi −12…+6 dB.
- **Final safety bersih:** tanh soft-clip dihapus. Final linked sample-peak safety ber-attack instan menjaga ceiling −0.8 dBFS dan melaporkan GR final ke meter.
- **QA baru:** `tools/prechain_headroom_audit.cpp` menguji seluruh 12 preset dengan dense hot program dan impulse/transient. Semua finite dan peak engine ≤0.89. Simulasi full chain pada level moderat settle **+3.99…+4.02 dB** untuk semua preset; hot master settle sesuai headroom sekitar **+0.64…+2.92 dB**, termasuk Night Listening +1.22 dB, tanpa sample clipping.

## v0.4.1 — 2026-07-10 — Smart Headroom Servo: Enak, Lebih Keras, Tidak Pernah Pecah
Regression audit v0.4.0 (suara pecah di banyak preset, Night Listening pelan). Root cause kuantitatif: **wow servo duduk SETELAH limiter engine** dan bisa menambah hingga +24 dB tanpa proteksi apa pun → hard digital clipping; servo juga tetap tracking saat preset fade (wet menuju 0 → desired gain melonjak). Engine sendiri sudah benar (smart headroom + makeup + limiter ber-ceiling) — wrapper yang mengalahkannya.
- **Servo kini headroom-aware.** Comp gain di-clamp oleh envelope peak wet (attack instan, release ~400 ms) terhadap safe ceiling −1 dBFS: servo mengejar +4 dB hanya sejauh headroom tersedia. Di hot master, lift settle di bawah +4 dB alih-alih pecah — persis perilaku "cari headroom dengan smart".
- **Servo dibekukan saat preset fade** + settle tail 8 blok, sehingga ganti preset tidak lagi memicu lonjakan gain.
- **Koreksi asimetris:** turun cepat (τ 60 ms, proteksi dulu) — naik pelan (τ 300 ms, tanpa pumping). Prime awal mendarat di min(desired, 0 dB) lalu glide naik, bukan lompat.
- **Absolute safety stage baru** sebagai stage terakhir sebelum audio keluar (setelah faucet OUTPUT): peak limiter 1.5 ms/110 ms ceiling −0.8 dBFS + tanh soft-clip knee di 0.985. Transparan penuh pada operasi normal; menjamin output tidak pernah crack apa pun kombinasi preset/macro/faucet.
- **QA `AskpDspAudit` diperluas: sweep seluruh 12 preset × 2 level program** (moderat & hot master, multitone 110/750/4200 Hz). Hasil: lift +3.65…+4.00 dB di level moderat untuk SEMUA preset (termasuk Podcast & Night Listening), peak maksimum 0.891 (ceiling −1 dBFS dihormati), nol sampel > 0.995. Hot master: +2.4…+3.9 dB, tetap bersih. Seluruh cek lama (true bypass bit-exact, isolasi preset↔I/O, recall A/B) tetap PASS.


## v0.4.0 — 2026-07-10 — Honest +4 dB Wow Gain, A/B Compare, Brand Header, Footer Air
- **ACTIVE = TRUE BYPASS + 4 dB, terkunci.** Audit kuantitatif menemukan lift default sebenarnya **+20.5 dB** (perceived-benefit engine + compressor makeup + excitation menumpuk di atas konstanta lama +5 dB). Konstanta statis diganti **wow reference servo**: wrapper mengukur slow-RMS dry (post input-faucet) vs wet lalu mengunci total lift ke tepat +4 dB via one-pole ~300 ms (bebas pumping, prime instan saat sinyal pertama, clamp ±24 dB, guard silence). Hasil terukur: **+3.99 dB** pada preset default. Bypass tetap raw bit-exact.
- **Knob INPUT/OUTPUT kini 0 dB di tengah.** Range disimetriskan −12…+12 dB (versi parameter di-bump ke 2 agar sesi lama tidak me-recall posisi normalised basi), double-click kembali ke 0 dB, dan keduanya **tidak lagi direset oleh ganti preset** — murni kontrol gain-match manual di sekitar referensi +4 dB.
- **Fitur A/B compare baru** di header: pill segmented A | B ala Linear. Klik sisi untuk berpindah snapshot; Alt+klik menyalin setelan aktif ke slot satunya (flash ring violet sebagai konfirmasi). Snapshot mencakup preset + semua macro + trim + lock + protect; **INPUT/OUTPUT dan BYPASS sengaja dikecualikan** supaya level matching bertahan di setiap flip. Kunjungan pertama ke slot kosong me-mirror setelan aktif (tanpa lompatan bunyi). Recall macro presisi diverifikasi tes.
- **Icon brand kini benar-benar muncul** di kiri atas: `drawBrandGlyph` sebelumnya memuat `askp_header_icon.png` (segitiga app solid) — kini memakai brand mark yang benar `askp_header_logo.png` (monogram "A" outline) 38 px dengan aura violet lembut.
- **Wordmark futuristik dua-tona:** "ARSON" silver gradient + "KUPIK" violet gradient, tracking lebar (kerning +0.24), halo violet tipis di belakang path glyph, dan hairline accent 1.4 px di bawah segmen violet. Dirender sebagai path glyph dengan gradient fill, bukan drawText polos.
- **Footer knob diberi napas:** 70×70 → 56×56 di dalam strip footer 72 px, margin 8 px atas-bawah sehingga tidak lagi menempel tepi chassis.
- **QA tool baru `AskpDspAudit`** (di bawah opsi snapshot tool): true-bypass bit-exact, lift default +4 dB, isolasi preset↔I/O, dan recall A/B — semuanya PASS. Verifikasi compile (Ninja/GCC, JUCE 8.0.4) + render snapshot offscreen di Linux.


## v0.1.7 — 2026-07-09 — Clean Sphere, Fluid Surface, Centered Layout
- **Sphere bersih dari garis noise.** Semua gloss & garis prosedural di dalam sphere dihapus: rim arcs, upper crown, lower caustic, window reflection, specular point, side streak, base glow, inner ring hairlines, ribbon arus internal, white hairline meniscus, dan dark refraction seam. Yang tersisa hanya edge vignette lembut (tanpa garis) untuk kesan cembung.
- **Glossy surface dari `nlyl-blue-circle.svg`.** Body biru SVG dibuat transparan penuh, hanya layer gloss (swoosh atas + radial glow bawah, dengan Gaussian blur asli) yang dirender rsvg → `askp_sphere_gloss.png` 640×640, di-mask ke elips bola, di-embed via BinaryData, dan digambar sebagai satu overlay di atas seluruh isi sphere (opacity 0.85 normal / 0.45 bypass).
- **Liquid kini fluida, bukan spectrum trace.** Sampling permukaan diturunkan 132 → 40 titik lalu dirender sebagai kurva quadratic-through-midpoints (front, back-rim, dan meniscus semuanya); smoothing spasial dinaikkan jadi dua pass kernel 5-tap. Puncak-puncak FFT kini menggulung seperti ombak.
- **Layout dipusatkan ke sumbu optik faceplate.** Sphere (300 px), macro knobs, dan keempat small knobs kini berbagi satu sumbu vertikal `faceAxis` = tengah area metal − 4 px; small knobs simetris persis ±92 px di sekitarnya. Ruang kosong atas hilang, seluruh cluster kontrol visually centered.
- **Hover & click feedback di semua kontrol.** GlassKnob: aura violet lembut saat hover, lebih kuat saat drag, arc & dot menyala + bloom di dot posisi. Chevron preset: tint violet + outline saat hover, nudge 1 px saat ditekan. BYPASS: outline glow saat hover, konten turun 1 px saat ditekan. Toggle PROTECT: ring violet saat hover, knob mengecil saat ditekan.
- QA (verifikasi compile + snapshot offscreen di Linux, JUCE 8.0.4): noise garis area kaca atas turun 3.99% → 0.30% area; roughness permukaan (mean |d²|) 8.61 → 4.14; gloss overlay terdeteksi tanpa blowout (max lum 153); liquid tetap violet di bawah overlay; pusat sphere terukur tepat (540, 284).

## v0.1.6 — 2026-07-09 — Liquid Glass Sphere
Rewrite total MasArray Sonic Core + polish UX pro:
- **Sonic Core = bola kaca berisi liquid.** Permukaan liquid kini benar-benar fill spectrum analyzer log-spaced **80 Hz – 16 kHz** (low kiri, high kanan; band edges dihitung dari sample rate host, bukan asumsi bin), dengan attack cepat/release lambat per band + spatial smoothing 5-tap agar permukaan terbaca sebagai fluida, bukan bin FFT bergerigi.
- **Kedalaman 3-D:** rim belakang liquid digambar sebagai band paralaks di atas permukaan depan sehingga cairan terlihat sebagai volume, bukan siluet datar; ditambah meniscus yang memanjat dinding kaca di tepi.
- **Glitter particles** — pool 76 partikel deterministik yang melayang naik di dalam cairan dengan twinkle individual (star-cross saat puncak kilau) + 7 gelembung naik; kecepatan & kecerahan mengikuti energi sinyal, membeku redup saat bypass.
- **Optik kaca:** fresnel rim light (key light kiri-atas + bounce violet kanan-bawah), refleksi jendela lebar miring, specular point panas, streak samping, caustic pool di dasar, seam refraksi gelap di bawah garis meniscus.
- **BYPASS pakai ikon power lucide** (garis + arc terbuka) dengan glow amber saat aktif; tombol dilebarkan 92→106 px.
- **Meter dB readable:** angka dB kini mengikuti garis peak-hold (latch → dwell ±1.5 s → fall 0.4 dB/frame), bukan nilai sesaat yang berkedip tak terbaca.
- Slosh liquid digerakkan energi low-band (bass surge) supaya cairan "bergejolak" musikal.
- Versi CMake & snapshot tool disinkronkan ke 0.1.6 (menutup version drift 0.1.3/0.1.5).
- QA: diverifikasi compile (Ninja/GCC, JUCE 8.0.4) + render snapshot offscreen di Linux; snapshot menunjukkan spectrum-fill bereaksi benar terhadap test tone 110 Hz + 3.4 kHz.

## v0.1.3-premium-ux-cleanup
- Removed thin vertical divider/separator lines from the main silver faceplate to reduce visual noise.
- Rebuilt the top header into a cleaner professional VST toolbar: brand, VST badge, functional preset browser, and real BYPASS only.
- Moved preset/mode selection exclusively to the top bar and removed the duplicate lower MODE strip.
- Enlarged the four secondary knobs slightly and pushed them outward from the MasArray Sonic Core for better breathing room.
- Kept all knobs in the same dark brushed-metal/violet-ring family so small knobs are scaled versions of the large knobs.
- Lowered UI refresh from 30 Hz to 24 Hz and added smoother attack/release/peak-hold behavior to the footer meters.
- Removed the horizontal oscilloscope line from the Sonic Core and replaced it with a smoother music-reactive liquid/plasma field inside the circular background.
- Simplified the Smart Protect strip label to reduce redundant text while keeping live gain-reduction readout and toggle.


## v0.1.2 — 2026-07-08
UX target lock-in: premium symmetrical MasArray surface.
- UI: redesigned into clean/lean target UX — silver raster faceplate, dark top/footer bars, symmetric ENHANCE and OUTPUT TRIM macro knobs, MasArray Sonic Core centered.
- UI: removed separate right black mode card; MODE and SMART PROTECT are now slim integrated strips.
- UI: all knobs now use one unified premium knob family; small knobs are scaled-down versions of the large knobs.
- UI: embedded raster surface assets (`askp_faceplate_brushed.png`, `askp_chassis_grain.png`) for more realistic premium material when canvas-only rendering is not enough.
- UX: VOCAL/STEREO/OUTPUT TRIM are now fully wired using the same APVTS slider attachments as the other controls, with clear value readouts.
- UX: Smart Protect toggle is custom-painted, not default JUCE toggle; Bypass state now mutes/desaturates knobs, mode strip, protect strip, and sonic core.
- Metering: footer meters now include a lightweight peak-hold marker.

## v0.1.1 — 2026-07-08
Perbaikan crackle saat knob diputar (kualitas kontrol setara plugin profesional):
- Engine: `rebuild_eq()` kini state-preserving & bebas alokasi — biquad EQ tidak lagi di-clear/realokasi setiap parameter berubah, sehingga state z1/z2 terjaga dan tidak ada diskontinuitas sinyal
- Engine: `butterworth_q_values()` memakai tabel statis (bebas alokasi di audio thread)
- Wrapper: 6 knob kontinu di-smooth 80 ms (SmoothedValue) dengan retune koefisien per-block — zipper-free
- Wrapper: ganti preset (topologi filter berubah) dibungkus micro-fade equal-power ±5 ms
- Smoke test baru `AskpCrackleTest`: sweep agresif → derivative identik statis (ratio 1.00x); hammer preset switch → max jump 0.0216 (setara sinyal natural). PASS
- Catatan: dua patch engine ini kandidat untuk di-upstream ke repo OBS (crackle yang sama ada di filter OBS saat slider digeser)

## v0.1.0 — 2026-07-08
- Port awal engine ArSonKuPik (OBS) ke VST3 + Standalone (JUCE 8.0.4, CMake)
- Engine DSP verbatim, parameter ID identik dengan filter OBS
- UI Galaxy-style: orb visualizer FFT (indikator aktif/bypass), ENHANCE knob besar, Smart Bass/Treble, panel MODE 12 preset, VOCAL/STEREO/TRIM, strip Smart Protect + GR live, meter I/O
- Font embedded: Plus Jakarta Sans, Sometype Mono
- Tool snapshot offscreen untuk QA visual

## v0.1.4 - Preset dropdown, stereo meters, liquid bowl visualizer
- Rebuilt the top preset browser as a proper dropdown ComboBox with prev/next arrows.
- Upgraded the footer to stereo L/R input and output metering.
- Reworked the centre visualizer into a circular glass bowl with luminous liquid spectrum motion.
- Reduced UI timer to 24 Hz and kept meter smoothing / slower peak-hold for lighter CPU feel.

## v0.1.5 - Liquid glass bowl, tighter layout, slimmer header/footer
- Shrunk the header height and made the preset panel / bypass button share the same compact height.
- Replaced chunky arrow buttons with clean chevron-style buttons.
- Rebalanced the whole face layout: smaller centre bowl, more breathing room around the small knobs, and less footer collision.
- Reworked the orb into a glass bowl with an empty upper chamber and a lower luminous liquid mass whose surface and internal ribbons react to the spectrum.
- Tightened the footer stereo meters so they stay professional without crowding the centre branding.

## v0.1.6b - Claude layout proportion and footer refinement
- Reduced the centre sphere slightly so the middle visual no longer dominates the faceplate.
- Enlarged the small knobs and pushed them farther away from the sphere for more professional spacing.
- Cleaned the footer meter layout and replaced the old clip accent with per-channel L/R clip boxes that latch red and reset on click.
- Removed the preset dropdown chevron-down icon to reduce visual noise while keeping dropdown behaviour intact.
- Added extra faceplate depth with sheen, edge shading, and a subtle centre wash so the panel feels less flat.

## v0.1.6c - Proportional layout and glass sphere realism pass
- Enlarged the secondary knobs again and moved SMART BASS / VOCAL upward to reduce the empty upper faceplate area.
- Re-centred the ENHANCE knob, OUTPUT TRIM knob, and sphere vertically within the metal faceplate.
- Reduced the sphere size slightly and added a stronger spherical-lens finish: edge falloff, refractive crown, lens glow, and lower caustic accent.
- Removed the preset ComboBox chevron-down arrow using a no-arrow ComboBox LookAndFeel while keeping dropdown behaviour.
- Removed the L/R clip boxes from the footer because Smart Protect already owns the clipping/protection story.
- Cleaned the footer meter spacing so the bottom strip reads more like a premium VST meter section.

## v0.1.7a - Panel lift and pink-noise visual calibration
- Lifted the full white-face control composition by roughly 8 px to reduce the remaining empty top space.
- Calibrated the orb spectrum visually for pink-noise injection by compensating the natural ~3 dB/oct tilt, so the liquid surface reads much flatter under pink noise.
- Added one extra spatial smoothing pass to keep the compensated surface clean and even.

## v0.1.8 - Preset knob sync + hybrid macro mapping engine
- Fixed preset selection so changing preset also updates Enhance, Smart Bass, Smart Treble, Vocal, Stereo, Output Trim, and Smart Protect UI values through APVTS-safe async macro sync.
- Fixed engine routing so live APVTS knob values are the source of truth after preset sync.
- Rebuilt macro mapping as hybrid architecture: Smart Bass / Smart Treble / Stereo are center-based around 50% RAW, while Enhance / Vocal are left-based additive macros from 0% RAW.
- Added center RAW marker only to Smart Bass, Smart Treble, Stereo, and Output Trim knobs; Enhance and Vocal no longer show the center marker.
- Implemented center-left Stereo narrowing path down to true mono at the far-left position.
- Reworked output makeup so additive macros create perceived loudness, not sectoral center positions.

## v0.1.9 - Output trim lock, unified UI font, popup spacing
- Added a lucide-style lock button to Output Trim. When locked, preset changes keep the current Output Trim value instead of resetting it.
- Added APVTS state parameter `output_trim_lock` so the lock state is saved/restored with sessions.
- Updated preset macro application to respect Output Trim lock.
- Unified plugin UI text rendering to the same Jakarta/default UI font style instead of mixing mono text in meters and labels.
- Customized the preset popup item renderer so the selected checkmark has a wider gutter and no longer crowds the preset name.
- Added stronger hover/click feedback for the new lock button.

## v0.2.0 - Vocal level fix, stronger bass/treble, trim architecture audit
- Fixed the Vocal macro losing level at high values. Vocal now uses gentler compression, stronger makeup, clearer mid-presence/body support, and less destructive anti-masking.
- Moved Output Trim out of the creative macro mapping. Trim is now applied in the limiter/output stage, so negative trim no longer makes the Vocal macro internally collapse.
- Increased Smart Treble enhancement: stronger air-band shelf, stronger presence lift, and more side-air sparkle.
- Increased Smart Bass slightly: stronger sub weight, punch body, and bass wet path.
- Retuned process_color internal compensation so vocal/presence enhancement no longer self-attenuates too aggressively.
- Added an engine audit test utility to compare macro loudness behaviour across vocal/trim/treble/bass cases.

## v0.2.1 - Output lock polish, centered vocal, Enhance loudness guard
- Redesigned the Output Trim lock as a small embedded lucide-style lock/unlock button under the Output Trim dB readout. Inactive state uses a dark button material; active state uses violet accent.
- Changed the header brand text to uppercase ARSONKUPIK using the same refined visual language as the centre/footer panel text.
- Changed Vocal from left-based additive to center-based macro: 50% neutral, right pushes vocal forward, left tucks/smooths vocal. The Vocal knob now shows a centre marker like the other center-based controls.
- Fixed Enhance 100% perceived loudness drop by making the compressor gentler, increasing smart makeup, and reducing over-aggressive peak/limiter penalties.
- Kept Output Trim isolated in the limiter/output stage so it does not change Vocal/Enhance tone behaviour.

## v0.2.2 - Header Output Lock placement
- Moved Output Trim Lock from inside the Output Trim knob area to the top header, directly left of the BYPASS button.
- Kept Output Trim Lock default OFF and preserved its preset-protection behavior.
- Replaced the previous custom lock drawing with one clean lucide-style locked icon shape for both ON and OFF states.
- OFF state now stays dark like the other header buttons; ON state uses violet accent.

## v0.2.3 - Brand logo and app icon
- Replaced the old circular header glyph with the provided ARSONKUPIK logo image in the top-left header.
- Added the same PNG as the standalone application icon (CMake icon settings + runtime top-level window icon).

## v0.2.4 - Build fix for runtime app icon
- Fixed the likely build failure from v0.2.3 by removing the unsafe TopLevelWindow::setIcon() call.
- Runtime app icon now uses ComponentPeer::setIcon() through the actual top-level component peer, which is safer across JUCE standalone/plugin wrappers.
- Header logo and embedded app-icon asset remain unchanged.

## v0.2.5 - Header brand + footer I/O knobs
- Replaced the top-left header brand image with the provided PNG logo asset.
- Switched the application build icon to the provided favicon.ico package and runtime window icon to the matching favicon PNG.
- Removed the VST chip from the header for a cleaner brand bar.
- Added new footer mini knobs at the far left and far right, following the reference layout: Input Gain on the left and a mirrored Output Trim mini knob on the right.
- Wired the new footer Input Gain knob as a real pre-engine gain stage, and the footer Output knob to the same output trim parameter as the main output trim knob.

## v0.2.6 - JUCE CMake icon configure fix
- Fixed the configure error caused by passing favicon.ico directly to JUCE CMake ICON_BIG / ICON_SMALL. JUCE expects PNG image sources and generates JuceLibraryCode/icon.ico itself.
- Added favicon-512x512.png as the big icon source and favicon-96x96.png as the small icon source.
- Kept runtime standalone window icon using the favicon PNG.
- Moved input gain target update before its smoothing skip so the footer input knob responds immediately and correctly.

## v0.2.7 - Runtime favicon BinaryData compile fix
- Removed the runtime BinaryData reference to favicon_96x96_png because JUCE may not generate that exact symbol for hyphenated filenames.
- Kept app icon handling through JUCE CMake ICON_BIG/ICON_SMALL, which generates the Windows app icon/resource safely.
- Header brand logo remains embedded from askp_app_logo.png.

## v0.2.8 - True footer input/output gain faucets
- Reworked the footer mini knobs so they render as larger, centred, premium glass knobs instead of tiny clipped controls.
- Input footer knob remains a true pre-chain input gain stage, before the smart DSP engine.
- Output footer knob is no longer a duplicate of Output Trim; it now controls a new independent Final Output Gain parameter after the DSP chain/limiter/trim and immediately before audio output.
- Added a wider attenuation range on the final output faucet (-36 dB to +6 dB) so processed output can be tamed even when Output Trim is already low.

## v0.2.9 - Raw bypass, visible header logo, refined footer I/O knobs
- Made BYPASS a true raw-audio path: no input faucet, no DSP engine, no output trim/limiter, no final output faucet, and no preset fade is applied while bypass is ON.
- Added a transparent header-logo asset generated from the provided app logo so the ARSONKUPIK mark is visible on the dark top bar.
- Refined footer input/output knobs: larger, vertically centred, thinner silver bezel, same glass style as the main knobs, and dB values shown in the knob face.
- Kept Input Gain as pre-chain only and Final Output Gain as the final post-chain output faucet only.
- Extended Final Output Gain down to -48 dB for stronger control when processed presets are still too hot.

## v0.3.0 - Active wow gain, preset trim lock fix, header icon, footer knob polish
- Fixed preset Output Trim behaviour: presets now write their own output trim value unless Output Trim Lock is ON.
- Added real tooltip support via TooltipWindow and clarified the Output Trim Lock tooltip.
- Reset footer Input/Output utility gains to 0 dB on preset changes so both footer knobs visually return to centre by default.
- Added a fixed +5.0 dB active-mode wow makeup after the DSP chain so active mode is clearly louder/more exciting than true raw bypass.
- Kept bypass as true raw audio: bypass does not pass through input gain, DSP, output trim, limiter, active makeup, or final output gain.
- Reworked the header brand icon into a transparent bright A icon so it is visible next to ARSONKUPIK on the dark top bar.
- Enlarged/polished footer Input/Output knobs and thinned their silver bezel for better proportion.
