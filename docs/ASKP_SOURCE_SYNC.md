# ASK-P authority migration note

This document is retained at its historical path because older releases linked to it.

Starting with **Sonkupik Live v0.12.0**, the mature ArSonKuPik Signature Engine v0.5.19 is vendored and hash-locked under:

```text
third_party/askp-engine-v0.5.19/
```

A sibling `askp-vst` checkout, `ASKP_SOURCE_DIR`, `-AskpSourceDir`, and `sync-askp-authority.ps1` are no longer used. This makes clean builds deterministic and prevents the Live sphere visualizer from changing when a separate ASK-P repository changes.

The approved Sonkupik sphere is independently owned by this repository under:

```text
third_party/sonkupik-sphere-v0.10.4/
```

Future engine upgrades must update `ASKP_AUTHORITY.lock`, the CMake SHA-256 gates, the bridge audit, and the preset/profile runtime audits together.
