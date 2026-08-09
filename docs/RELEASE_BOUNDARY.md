# SC220 Live release boundary

## Private source authority

`masarray/sc220-live` is the only canonical application-source repository. It may contain C++, CMake, build and release automation, product documentation, intentional UI assets, the installer template, third-party verification rules, and the pinned private ASK-P submodule reference. Generated binaries, downloaded driver packages, debug symbols, build trees, local dependencies, signing credentials, and release staging directories are ignored and rejected by release preflight.

The repository never commits `VBCABLE_Driver_Pack45.zip`, `VBCABLE_Setup_x64.exe`, driver catalogues, or other VB-Audio binaries. The installer workflow downloads the standard package only from the official VB-Audio URL and accepts it only after matching the reviewed `VBCABLE_PACK45_SHA256` repository variable and validating Authenticode/catalogue signatures.

## Public download authority

`masarray/sc220-download` may contain only:

- the static download landing page;
- public brand artwork approved for that page;
- `.nojekyll`, public README, and version metadata;
- compiled SC220 Live installer and optional portable package as GitHub Release assets;
- SHA-256 checksum files;
- release supply-chain manifest recording the exact private source commit and verified standard VB-CABLE identity.

The page branch does not contain the SC220 executable, VB-CABLE installer, application source, driver files, build scripts, debug symbols, certificates, or private keys. Compiled assets live only on the GitHub Release.

GitHub automatically offers source archives for the public release tag. Those archives contain only the public landing-page repository, never the private application repository or VB-Audio package.

## Third-party installer boundary

The public installer may embed only the standard VB-Audio VB-CABLE setup under the published VB-Audio donationware distribution conditions. It must:

- preserve VB-Audio identity and ownership;
- present the donationware notice and official information/donation links;
- keep VB-CABLE installation optional;
- keep the Windows driver-consent prompt visible;
- request restart after successful driver installation;
- embed the reviewed setup unchanged;
- reject VB-CABLE A+B, C+D, or any other paid/personal-download package.

VB-CABLE remains a separate product. SC220 Live uninstall does not silently remove or modify it.

## Proprietary distribution contract

SC220 Live application source is proprietary and remains private. Release preflight requires `LICENSE`, `EULA.txt`, and `PURCHASE_TERMS.txt` to declare the 365-day free-control period, safe audio continuation, optional IDR 399,000 one-time activation, and third-party rights. Public binary packages include those user-facing terms and notices.

## Publication invariants

- Private source repository visibility must read back as `PRIVATE`.
- Public download repository visibility must read back as `PUBLIC`.
- The source commit approved for release must exactly match clean `origin/main`.
- Installer and portable staging must contain no source or debug artifacts.
- Third-party driver download must match the reviewed SHA-256 and valid signatures.
- The public page branch must contain no application or driver binaries.
- Installer, checksum, and manifest must exist on the public release.
- The published release must be marked latest.
