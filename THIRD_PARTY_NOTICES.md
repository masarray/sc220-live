# Third-party notices

SC220 Live itself is proprietary. The following components retain their own
licence terms and ownership.

## JUCE 8.0.14

SC220 Live is built with JUCE 8.0.14 under an applicable JUCE closed-source
distribution licence. JUCE remains owned by Raw Material Software Limited. No
right to use or redistribute JUCE separately is granted by SC220 Live.

## Plus Jakarta Sans

Plus Jakarta Sans Regular and SemiBold are embedded in the user interface under
the SIL Open Font License 1.1. The complete text is stored at
`assets/fonts/OFL.txt` in the private source authority.

## Lucide Icons

A focused subset of Lucide SVG icons is embedded under the ISC licence. The
complete text is stored at `assets/icons/LUCIDE_LICENSE.txt` in the private
source authority.

## ASK-P Signature Engine v0.5.23

The embedded ASK-P Signature DSP, profile bank, headless processor, and local
activation foundation are proprietary materials supplied by and owned by
Tutorial Mas Ari / MasArray. SC220 Live pins the reviewed authority commit
`5757119cab7a1a75f09dcf0edea6e3be97975439`.

## VB-Audio VB-CABLE standard

The Windows installer may include the standard VB-Audio VB-CABLE driver as an
optional component. VB-CABLE is developed, owned, and distributed by VB-Audio
Software under its donationware model. The installer bundle uses the official
`VBCABLE_Driver_Pack45.zip`, verifies a reviewed SHA-256 and the Windows
Authenticode/catalogue signatures, and embeds `VBCABLE_Setup_x64.exe` unchanged.

End users can identify VB-CABLE as a VB-Audio product and remain free to donate
or purchase a licence if they find it useful:

- Product and download information: https://vb-audio.com/Cable/
- Donation/licensing: https://shop.vb-audio.com/en/win-apps/11-vb-cable.html

VB-CABLE is a separate product. Inclusion does not imply sponsorship,
endorsement, support, or affiliation between VB-Audio and SC220 Live, Tutorial
Mas Ari, MasArray, or Recording Tech. The public bundle does not include
VB-CABLE A+B or C+D.

## ArSonEQ, ArSonComp and ArSonVerb sibling VST3 bundles (P16 engineering preview)

The three standalone plugin bundles are independently published and offered
under AGPL-3.0. ArSonEQ M3.1 is an **unmerged engineering prerelease**;
ArSonComp v0.2.0 and ArSonVerb v1.0.1 are sibling product versions.
The P16 Windows preview fetches immutable release assets using SHA256, includes
the sibling repository source and licence provenance, and loads them as ordinary
VST3 modules via the SC220 audio host. They are not compiled into the SC220
executable, do not share source with the ASIO real-time callback, and retain
their own copyright and licence notices.

- ArSonEQ: https://github.com/masarray/arson-eq/tree/1f9513d75faea306e06bafd7b68119d4b8bef276
- ArSonComp: https://github.com/masarray/arson-comp/tree/0a039ef6d340e26aa01a62df3670f74da1f03a12
- ArSonVerb: https://github.com/masarray/arsonverb/releases/tag/v1.0.1

**Distribution review needed**: the SC220 host is currently marked proprietary,
while each independent VST3 is AGPL-3.0. Separate plugin source and licence
notices do not automatically resolve every potential combined-work/copyright
issue. Before stable or commercial redistribution, obtain a documented
licensing determination/exception as appropriate. This integration is for
explicitly labelled engineering preview and hardware testing only.
