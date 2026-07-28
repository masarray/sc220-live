# Sonkupik Live v0.13.0 — 30-Day Trial and QRIS Licensing

## Product policy

- Full-feature trial: 30 days from the first launch on a computer.
- Trial warning: the header changes colour during the final 7 days.
- After expiry: audio keeps running with the current/default processing state, while creative controls become read-only.
- Full Version: one-time perpetual licence for one computer.
- Price: Rp249.000.
- Checkout: hosted by Midtrans QRIS through the MasArray licence server.
- Offline fallback: signed activation code bound to the Computer Request ID.

## Security model

The executable contains only the RSA public key. The private signing key and Midtrans credentials remain on the licence server.

The checkout worker runs on a low-priority background thread. No networking, JSON parsing, RSA verification, browser launching, registry access, or file persistence runs in the realtime audio callback.

The request ID keeps the existing `ASKP-R1-...` transport format for server compatibility, but Sonkupik Live uses a product-specific hardware fingerprint salt and separate state locations. An ArSonKuPik VST activation therefore does not unlock Sonkupik Live.

Local trial state is mirrored in:

- `%APPDATA%/MasArray/Sonkupik Live/licence-state-v1.dat`
- `HKCU/Software/MasArray/Sonkupik Live/`

A clock rollback beyond the existing tolerance changes the licence state to `ClockNeedsAttention` rather than extending the trial.

## Checkout contract

The client sends:

```json
{
  "product_id": "sonkupik-live",
  "device_request_id": "ASKP-R1-...",
  "customer_name": "...",
  "customer_email": "..."
}
```

The server must return an order with `amount_idr` exactly `249000`. The client refuses to open QRIS when the returned amount differs.

Only HTTPS checkout URLs hosted by `app.midtrans.com` or `app.sandbox.midtrans.com` are accepted.

## User interface

The header licence button shows one of:

- `TRIAL • 30 DAYS`
- `UNLOCK • Rp249K`
- `CHECK CLOCK`
- `FULL VERSION`

The native Sonkupik Live purchase panel supports:

- customer name and receipt email;
- QRIS checkout creation;
- restoring a saved unpaid order;
- automatic payment polling;
- payment recovery after restart;
- automatic signed activation;
- manual payment refresh;
- cancellation of verified unpaid orders;
- offline request ID and signed activation-code entry.
