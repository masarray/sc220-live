# Sonkupik Live v0.13.3 — Device and Evaluation UX Fix

## Evaluation presentation

The active evaluation period is intentionally quiet. No countdown or trial badge is shown in the main header. The header displays `TRIAL ENDED` only when the 30-day evaluation has actually expired, or `CHECK CLOCK` when the local clock requires attention.

Licensing remains available through **Settings → License & Activation**. The licensing panel uses the neutral `FULL FEATURES ACTIVE` state during evaluation and avoids non-ASCII separators that could render as mojibake on Windows.

## Device switching

Input and output selection now use a verified transaction:

1. Scan the current audio-device type.
2. Apply the requested endpoint while preserving the current sample rate and buffer.
3. If the pair is incompatible, retry with automatic sample-rate and buffer negotiation.
4. If a Windows driver remains busy, close and reopen the pair once.
5. Roll back to the previous working setup when every attempt fails.

The UI queues the transaction after the native combo popup closes, disables both selectors during the reopen, and suppresses timer-driven text restoration until the transaction completes.
