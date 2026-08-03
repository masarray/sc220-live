# SC220 Live release boundary

## Private source authority

`masarray/sc220-live` is the only canonical application-source repository. It may contain C++, CMake, build and release automation, product documentation, intentional UI assets, and the pinned private ASK-P submodule reference. Generated binaries, debug symbols, build trees, local dependencies, signing credentials, and release staging directories are ignored and rejected by release preflight.

## Public download authority

`masarray/sc220-download` may contain only:

- the static download landing page;
- public brand artwork approved for that page;
- `.nojekyll`, public README, and version metadata;
- portable binary ZIP and SHA-256 checksum as GitHub Release assets.

The release script rejects C/C++ source, headers, CMake inputs, executables committed directly to the page branch, linker/compiler products, debug symbols, certificates, and private keys.

GitHub automatically offers source archives for the public release tag. Those archives contain only this public landing-page repository, never the private application repository.

## Proprietary distribution contract

SC220 Live application source is proprietary and remains private. Release
preflight requires `LICENSE`, `EULA.txt`, and `PURCHASE_TERMS.txt` to declare the
365-day free-control period, safe audio continuation, optional IDR 399,000
one-time activation, and third-party rights. Public binary packages include
those user-facing terms and notices.

## Publication invariants

- Private source repository visibility must read back as `PRIVATE`.
- Public download repository visibility must read back as `PUBLIC`.
- The source commit pushed to `origin/main` must match local `HEAD`.
- Release ZIP staging must contain no source or debug artifacts.
- The public branch must contain no application source or binaries.
- Both ZIP and checksum must exist on the public release.
- The published release must be marked latest.
