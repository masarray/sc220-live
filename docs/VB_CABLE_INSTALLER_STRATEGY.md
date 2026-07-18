# VB-CABLE Installer and Beginner Routing Strategy

## Goal

A non-technical streamer should reach this working path with minimal manual setup:

```text
Audio interface -> Sonkupik Live -> CABLE Input
OBS/TikTok audio source <- CABLE Output
```

Sonkupik Live does not create a kernel driver. VB-CABLE remains a separately identified
third-party component, avoiding the cost and maintenance of publishing a new virtual-audio driver.

## Current source milestone

Implemented:

- Detect/select available output endpoints.
- Prefer a VB-CABLE-looking output on first run.
- Display `VB-CABLE Ready` only when the selected output matches a cable endpoint.
- Preserve the selected input/output device state.
- Provide actionable output setup rather than silently changing Windows microphone defaults.

Not implemented in v0.8.0:

- Bundled VB-CABLE installer payload.
- Silent/elevated driver installation.
- Automatic reboot orchestration.
- Production uninstall/repair workflow.

## Planned first-run flow

```text
Start installer
  -> explain that VB-CABLE is a third-party donationware component
  -> offer Install VB-CABLE
  -> request UAC only for the driver step
  -> install the approved official package
  -> request reboot when required
  -> launch Sonkupik Live
  -> choose audio-interface input
  -> select CABLE Input as app output
  -> show OBS/TikTok instruction: choose CABLE Output
  -> run signal test
```

Do not change the Windows default microphone. The application only needs its own selected input
and output endpoints. This prevents Zoom/Discord or other apps from unexpectedly receiving the
computer mix.

## Release gates before bundling

1. Re-check current VB-Audio redistribution and donation/licensing terms.
2. Preserve VB-Audio name, ownership and donation notice.
3. Bundle only the specifically permitted standard package.
4. Verify silent/elevated installer switches against the exact package version.
5. Verify install, repair, upgrade and uninstall on clean Windows 10/11 machines.
6. Never delete or replace a user-installed cable variant without explicit consent.
7. Provide a manual official-source path if automatic installation fails.
8. Keep Sonkupik Live usable when VB-CABLE is already installed.

## Failure-safe UX

- `VB-CABLE not detected` is a setup state, not a generic red crash error.
- Keep the selected audio interface visible.
- Offer `Rescan`, `Open Setup Guide` and later `Install VB-CABLE` actions.
- Do not start output streaming until a valid render endpoint is open.
- On uninstall, remove only Sonkupik Live settings/files unless the user separately chooses to
  remove VB-CABLE.

## Signing boundary

The signed VB-CABLE driver and the Sonkupik Live application are separate trust surfaces.
Bundling an already signed third-party driver does not automatically sign the Sonkupik Live
installer/executable. Public releases must still account for SmartScreen reputation or an
application code-signing strategy, but they do not require MasArray to publish a new kernel driver.
