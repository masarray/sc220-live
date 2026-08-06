# Local Runner dan Release SC220 Live

## Tujuan

Alur ini memindahkan kompilasi installer SC220 Live dari runner Windows milik GitHub ke PC Windows sendiri. Setelah setup pertama selesai, satu shortcut dapat:

1. memilih versi rilis;
2. memastikan source sudah di-push;
3. menyalakan runner lokal hanya selama sesi release;
4. membangun smart installer pada PC lokal;
5. menjalankan audit serta smoke install/uninstall;
6. membuat GitHub Release pada `masarray/sc220-download`;
7. mengunggah installer, checksum, manifest, dan catatan authority;
8. memperbarui `sc220-download/latest.json` agar tombol download situs mengarah ke versi baru;
9. mengembalikan runner ke kondisi offline.

## File utama

- `SETUP-LOCAL-RUNNER.cmd` — setup pertama kali.
- `RELEASE-SC220.cmd` — build dan release berikutnya.
- `.github/workflows/local-release.yml` — workflow manual yang hanya menerima runner berlabel `sc220-build`.
- `scripts/configure-sc220-runner-service.ps1` — membatasi service ke mode release-only.
- `scripts/publish-sc220-download.ps1` — validasi dan publikasi release.
- `qa/audit_sc220_local_release.py` — menjaga kontrak keamanan alur release.

## Syarat sebelum release pertama

File `.github/workflows/local-release.yml` harus sudah tersedia pada default branch `main`. Launcher selalu menjalankan definisi workflow yang stabil dari `main`, tetapi input `source_ref` boleh menunjuk branch atau tag lain yang sudah di-push.

Dengan pemisahan ini, branch pengembangan yang dipilih tetap dapat dibangun tanpa mengambil definisi workflow dari branch tersebut. Merge perubahan local-runner ke `main` terlebih dahulu sebelum menjalankan `RELEASE-SC220.cmd` untuk pertama kali.

## Setup pertama kali

Checkout branch yang memuat fitur ini, lalu double-click:

```text
SETUP-LOCAL-RUNNER.cmd
```

Seluruh rangkaian meminta hak Administrator dan kemudian:

- memeriksa atau memasang Git, GitHub CLI, Python, CMake, Ninja, Inno Setup, serta Visual Studio C++ Build Tools;
- membuka browser untuk login GitHub bila GitHub CLI belum login;
- mendaftarkan runner khusus repository `masarray/sc220-live`;
- memasangnya pada `C:\actions-runner-sc220` dengan label tambahan `sc220-build`;
- mengubah service agar memakai akun `LocalSystem` dan startup `Manual`;
- menulis snapshot machine-level `PATH` ke `C:\actions-runner-sc220\.env`;
- memulai service hanya untuk verifikasi host, lalu menghentikannya kembali;
- membuat shortcut desktop `Release SC220 Live`.

Instalasi Visual Studio Build Tools dapat berukuran besar. Jangan mematikan PC atau koneksi internet selama setup pertama.

## Mengapa service memakai LocalSystem

Smoke test memasang dan menghapus installer Inno Setup yang secara default membutuhkan hak administrator. Karena itu runner release memerlukan hak tinggi. Untuk mengurangi risikonya:

- runner terikat hanya ke repository private `masarray/sc220-live`;
- workflow hanya memiliki trigger manual `workflow_dispatch`;
- tidak ada trigger push, pull request, schedule, atau repository publik;
- service disetel `Manual`, bukan otomatis saat Windows menyala;
- `RELEASE-SC220.cmd` menyalakan runner sesaat sebelum job dan menghentikannya dalam blok `finally` setelah job berhasil maupun gagal;
- definisi workflow selalu berasal dari `main`;
- token publikasi hanya tersedia pada step terakhir.

Konsekuensinya, jangan memberi akses write/admin repository kepada pihak yang tidak dipercaya dan jangan menambahkan workflow PR otomatis ke runner ini. Proses yang berjalan sebagai `LocalSystem` memiliki akses tinggi ke PC.

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

Launcher meminta hak Administrator lalu:

- menolak working tree yang masih kotor;
- memastikan source lokal sama dengan source remote yang dipilih;
- memastikan workflow release tersedia pada `main`;
- memastikan service runner berada pada mode `LocalSystem` + `Manual`;
- membaca versi terbaru dari `sc220-download/latest.json`;
- menyarankan patch version berikutnya;
- meminta channel `stable` atau `prerelease`;
- meminta konfirmasi final dengan mengetik `RELEASE`;
- menyalakan service dan menunggu runner `sc220-build` online;
- menjalankan serta memantau workflow sampai selesai;
- menghentikan service runner pada akhir sesi.

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
- Definisi workflow selalu diambil dari branch `main`, sedangkan source build diberikan sebagai input terpisah.
- Runner terdaftar pada satu repository, bukan seluruh organisasi.
- Runner berhak tinggi tetapi offline di luar sesi release.
- Checkout tidak menyimpan credential GitHub ke konfigurasi Git lokal.
- `SC220_DOWNLOAD_TOKEN` hanya diberikan kepada step publikasi.
- Workflow tidak mengunggah artifact sementara ke storage Actions.
- VB-CABLE tidak dipasang pada smoke test; driver hanya ikut di dalam installer final.
- Folder checkout private ASK-P dan paket sementara dibersihkan setelah job.

Jangan menambahkan trigger pull request ke workflow self-hosted ini. Kode dari kontribusi yang belum dipercaya tidak boleh otomatis dijalankan pada PC lokal.

## Operasional

Untuk memeriksa service:

```powershell
Get-Service 'actions.runner.*'
```

Di luar sesi release, status normal adalah `Stopped` dengan startup `Manual`. Runner dapat didaftarkan ulang dengan menjalankan kembali `SETUP-LOCAL-RUNNER.cmd`.

PC harus tetap menyala dan tersambung internet sampai `RELEASE-SC220.cmd` menyatakan proses selesai. Menutup jendela release di tengah job dapat meninggalkan job gagal; service tetap diusahakan berhenti oleh blok cleanup saat script keluar normal.

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
