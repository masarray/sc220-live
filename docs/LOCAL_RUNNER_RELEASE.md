# Local Runner dan Release SC220 Live

## Tujuan

Alur ini memindahkan kompilasi installer SC220 Live dari runner Windows milik GitHub ke PC Windows sendiri. Setelah setup pertama selesai, satu shortcut dapat:

1. memilih versi rilis;
2. memastikan source sudah di-push dan runner sedang online;
3. membangun smart installer pada PC lokal;
4. menjalankan audit serta smoke install/uninstall;
5. membuat GitHub Release pada `masarray/sc220-download`;
6. mengunggah installer, checksum, manifest, dan catatan authority;
7. memperbarui `sc220-download/latest.json` agar tombol download situs mengarah ke versi baru.

## File utama

- `SETUP-LOCAL-RUNNER.cmd` — setup pertama kali.
- `RELEASE-SC220.cmd` — build dan release berikutnya.
- `.github/workflows/local-release.yml` — workflow manual yang hanya menerima runner berlabel `sc220-build`.
- `scripts/publish-sc220-download.ps1` — validasi dan publikasi release.
- `qa/audit_sc220_local_release.py` — menjaga kontrak keamanan alur release.

## Setup pertama kali

Checkout branch yang memuat fitur ini, lalu double-click:

```text
SETUP-LOCAL-RUNNER.cmd
```

Script meminta hak Administrator dan kemudian:

- memeriksa atau memasang Git, GitHub CLI, Python, CMake, Ninja, Inno Setup, serta Visual Studio C++ Build Tools;
- membuka browser untuk login GitHub bila GitHub CLI belum login;
- mendaftarkan runner khusus repository `masarray/sc220-live`;
- memasangnya sebagai Windows service pada `C:\actions-runner-sc220`;
- memberi label tambahan `sc220-build`;
- membuat shortcut desktop `Release SC220 Live`.

Instalasi Visual Studio Build Tools dapat berukuran besar. Jangan mematikan PC atau koneksi internet selama setup pertama.

## Token yang diminta

### `ASKP_REPO_TOKEN`

Hanya diperlukan bila secret ini belum ada di `sc220-live`.

Buat fine-grained personal access token dengan:

- repository access: hanya `masarray/askp-vst`;
- repository permission: **Contents — Read-only**.

### `SC220_DOWNLOAD_TOKEN`

Buat fine-grained personal access token dengan:

- repository access: hanya `masarray/sc220-download`;
- repository permission: **Contents — Read and write**.

Token diketik pada prompt tersembunyi dan langsung dikirim ke GitHub sebagai encrypted Actions secret. Script tidak menulis token ke source code atau file teks lokal.

## Membuat release

Pastikan perubahan yang hendak dirilis sudah di-commit dan di-push. Kemudian double-click shortcut desktop atau:

```text
RELEASE-SC220.cmd
```

Launcher akan:

- menolak working tree yang masih kotor;
- memastikan branch lokal sama dengan branch remote;
- membaca versi terbaru dari `sc220-download/latest.json`;
- menyarankan patch version berikutnya;
- memeriksa runner `sc220-build` sedang online;
- meminta channel `stable` atau `prerelease`;
- meminta konfirmasi final dengan mengetik `RELEASE`;
- menjalankan dan memantau workflow sampai selesai.

Release baru tidak mengganti tag dengan versi yang sama secara diam-diam. Gunakan nomor versi baru apabila tag tersebut sudah ada.

## Gate sebelum publikasi

Release hanya dibuat setelah seluruh tahap berikut berhasil:

1. source dan authority lock tervalidasi;
2. ASK-P authority di-checkout pada commit yang dipin;
3. kontrak smart installer dan local release lulus audit;
4. aplikasi dan installer berhasil dikompilasi;
5. installer berhasil dipasang secara silent tanpa mengubah driver VB-CABLE;
6. prerequisite probe dari payload terpasang menghasilkan status sehat;
7. uninstaller berhasil dijalankan;
8. checksum dan manifest cocok dengan installer final.

Jika salah satu tahap gagal, script publikasi tidak dijalankan dan `latest.json` tidak diubah.

## Batas keamanan

- Workflow hanya memiliki trigger `workflow_dispatch`; tidak berjalan pada push atau pull request.
- Runner terdaftar pada satu repository, bukan seluruh organisasi.
- Checkout tidak menyimpan credential GitHub ke konfigurasi Git lokal.
- `SC220_DOWNLOAD_TOKEN` hanya diberikan kepada step publikasi.
- Workflow tidak mengunggah artifact sementara ke storage Actions.
- VB-CABLE tidak dipasang pada smoke test; driver hanya ikut di dalam installer final.
- Folder checkout private ASK-P dan paket sementara dibersihkan setelah job.

Jangan menambahkan trigger pull request ke workflow self-hosted ini. Kode dari kontribusi yang belum dipercaya tidak boleh otomatis dijalankan pada PC lokal.

## Operasional

PC harus menyala, tersambung internet, dan service GitHub runner harus berstatus Running saat release dijalankan. Untuk memeriksa service:

```powershell
Get-Service 'actions.runner.*'
```

Runner dapat didaftarkan ulang dengan menjalankan kembali `SETUP-LOCAL-RUNNER.cmd`. Script akan mencoba melepas registrasi lama, menghentikan service lama, lalu memasang konfigurasi baru.

## Hasil publikasi

Release `vX.Y.Z` pada `masarray/sc220-download` berisi:

- `SC220-Live-vX.Y.Z-Setup-win-x64.exe`
- file `.sha256`
- manifest JSON
- `BUILD_COMMIT.txt`
- `VC_REDIST_X64_SHA256.txt`
- `VB_CABLE_PACK45_SHA256.txt`

Setelah release terverifikasi, publisher memperbarui:

- `latest.json`
- `RELEASE_NOTES_vX.Y.Z.md`

Commit tersebut memicu deployment halaman download publik yang sudah ada pada repository `sc220-download`.
