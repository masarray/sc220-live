# Licence server product configuration - SC220 Live

The existing MasArray licence service must expose this exact catalogue entry
before the purchase button is considered production-ready:

```text
product_id       = sc220-live
name             = SC220 Live Full Version
amount_idr       = 399000
licence_kind     = perpetual creative-control activation
active_computers = 1
payment_method   = hosted Midtrans QRIS
```

The desktop client sends this product ID to `POST /v1/checkout`, rejects any
response whose product or `amount_idr` differs, and validates only signed
activation codes bound to its SC220-specific Computer Request ID. The service
must use the same RSA signing authority and `ASKP-A1` activation envelope as
ASK-P v0.5.23.

Do not enable paid production checkout until merchant identity, final amount,
refund terms, product catalogue, and recovery behavior have been verified on
the deployed service. A source audit proves client behavior; it does not prove
that the remote product catalogue is configured.

The public release script requires `/health` to report `environment: production`
and the product endpoint to report IDR 399,000, `perpetual`, and one activation.
It intentionally stops before publication when any of these checks fails.
