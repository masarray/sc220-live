# Local QA dan Public Draft Release SC220 Live

## Kebijakan

Repository private `masarray/sc220-live` tidak memiliki GitHub Actions. Source ASK-P,
kompilasi, QA, dan packaging hanya dijalankan pada workstation Windows lokal.
GitHub automation hanya boleh berjalan di repository publik
`masarray/sc220-download`, setelah menerima draft release berisi binary allowlist.

## Alur release

Jalankan:

```text
RELEASE-SC220.cmd
```

Launcher lokal akan:

1. menolak branch selain `main`, working tree kotor, atau HEAD yang berbeda dari
   `origin/main`;
2. menjalankan clean Release build dan seluruh source/FFT/compiled QA melalui
   `scripts/run-sc220-local-qa.ps1`;
3. membangun installer lokal melalui `scripts/package-sc220-installer.ps1`;
4. membuat draft release yang belum dipublikasikan di `masarray/sc220-download`;
5. mengunggah hanya enam aset yang diizinkan;
6. men-dispatch `publish-draft-release.yml` pada repository publik dan menunggu
   hasilnya;
7. membaca kembali release dan metadata `latest` setelah workflow publik sukses.

## Binary allowlist

Draft publik hanya boleh memuat:

- `SC220-Live-vX.Y.Z-Setup-win-x64.exe`
- `SC220-Live-vX.Y.Z-Setup-win-x64.exe.sha256`
- `SC220-Live-vX.Y.Z-Setup-win-x64.manifest.json`
- `BUILD_COMMIT.txt`
- `VC_REDIST_X64_SHA256.txt`
- `VB_CABLE_PACK45_SHA256.txt`

Tidak ada checkout, source, generated DSP, build tree, debug symbol, credential,
atau signing material yang dikirim dari repository private.

## Otoritas publikasi

`scripts/publish-sc220-download.ps1` tidak mempublikasikan draft secara langsung
dan tidak melakukan push ke tree publik. Workflow publik
`sc220-download/.github/workflows/publish-draft-release.yml` menjadi satu-satunya
otoritas publikasi. Workflow tersebut memverifikasi allowlist aset, checksum,
manifest, payload Inno Setup, ekstensi source/debug/key terlarang, dan marker
private ASK-P sebelum mengubah draft menjadi release publik.

Jika salah satu gate gagal, draft harus tetap tidak dipublikasikan.

## Prasyarat lokal

- Visual Studio C++ workload dan Windows SDK
- CMake dan Ninja
- Python; audit FFT memerlukan NumPy
- Inno Setup yang dipin oleh packaging contract
- GitHub CLI yang sudah login dan memiliki write access ke `sc220-download`
- authority locks VC++ Runtime dan VB-CABLE yang valid

Tidak diperlukan self-hosted Actions runner, Actions secret untuk ASK-P, atau
service `actions.runner.*`.
