# SC220 Live licensing contract

SC220 Live is proprietary software distributed publicly as compiled Windows
binaries only. The private `masarray/sc220-live` repository is the source
authority; `masarray/sc220-download` is a public download surface and must never
contain the application source, CMake project, debug symbols, or signing data.

## User experience

- First launch starts 365 days of free, full creative control on that computer.
- No card is required, and there is no automatic payment or subscription.
- After day 365, audio processing, meters, input/output routing, faders, mute,
  ducking, and normal streaming continue to work.
- ASK-P preset selection and its five creative macro controls become read-only.
- An optional one-time IDR 399,000 activation restores creative editing for one
  active computer.

## Technical authority

The implementation reuses the reviewed ASK-P v0.5.23 state machine, RSA public
key validation, `ASKP-R1` request transport, `ASKP-A1` activation transport,
clock rollback detection, and mirrored file/registry state. SC220 Live uses a
different state salt, hardware-fingerprint salt, registry path, application-data
path, checkout product ID, and checkout state directory. An ASK-P VST activation
therefore does not automatically unlock SC220 Live.

Checkout and payment verification run on a background thread and are never
called from the realtime audio path. Pending order capability data is stored
locally so an interrupted payment can be checked again without creating a
duplicate order.

Run `scripts/check-sc220-release-contract.ps1` to audit these invariants.
The one-click public release additionally verifies the deployed `/health` and
`/v1/products/sc220-live` endpoints and refuses sandbox or mismatched payment
configuration.
