# Sonkupik Live v0.13.2 — Source Delivery Summary

This source package ports the supplied ArSonKuPik v0.5.19 Signature Engine into
Sonkupik Live through a deterministic embedded engine boundary.

## Key deliverables

- `live/EnhancerEngineBridge.h/.cpp`
- vendored v0.5.19 engine and 13-profile bank
- headless v0.5.19 processor port
- independent local v0.10.4 Sonkupik sphere
- hash-locked CMake authority checks
- single-click Windows build without a sibling ASK-P checkout
- optional 80 ms reference versus 12 ms Live parity renderers

## Build

```bat
build-live-windows.bat clean
```

## Authority evidence

- `ASKP_AUTHORITY.lock`
- `docs/ENHANCER_ENGINE_PORT_v0.12.0.md`
- `qa/v0120_engine_port_audit.txt`


## v0.13.0 licensing additions

- `live/LicenseCheckoutClient.*`
- `live/LiveLicensePanel.*`
- product-specific 30-day `LicenseManager`
- QRIS product ID `sonkupik-live`, price Rp249.000
- server configuration contract under `docs/`
