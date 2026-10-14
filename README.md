# TermWatch

Monitor sistem dan proses real-time untuk Linux, ditulis dalam C murni tanpa library eksternal. Tampilan dan kemampuannya terinspirasi btop.

## Fitur

| Area | Detail |
|---|---|
| CPU | Grafik braille riwayat pemakaian, bar total, bar per-core, frekuensi, suhu, load average, uptime |
| Memori | Used, available, cached, free, swap |
| Disk | Pemakaian tiap mount point dan kecepatan baca/tulis |
| Jaringan | Grafik download/upload per interface, kecepatan, puncak, total, alamat IP |
| Proses | Pid, program, command, thread, user, memori, CPU |
| Urut dan cari | Urut menurut kolom mana pun, balik urutan, filter nama, command, user, atau pid |
| Tree | Tampilan pohon proses dengan collapse dan expand |
| Aksi | Detail proses, kirim SIGTERM, SIGKILL, atau sinyal pilihan |
| Mouse | Scroll, klik baris, klik header untuk mengurutkan |

## Build dan jalankan

```
make
./termwatch
```

Butuh terminal dengan UTF-8 dan truecolor, ukuran minimal 60x16.

## Tombol

| Tombol | Fungsi |
|---|---|
| ↑ ↓ PgUp PgDn Home End | Pindah pilihan proses |
| ← → | Ganti kolom pengurutan |
| r | Balik urutan |
| e | Tampilan tree |
| spasi | Collapse atau expand cabang (mode tree) |
| c | Persen CPU per-core |
| f | Filter (enter simpan, esc hapus) |
| enter atau i | Detail proses |
| t / k / s | SIGTERM / SIGKILL / pilih sinyal |
| b / n | Interface jaringan sebelumnya / berikutnya |
| z | Reset total jaringan |
| + / - | Ubah interval pembaruan (100 sampai 10000 ms) |
| 1 2 3 4 | Tampilkan atau sembunyikan cpu, mem, net, proc |
| h, ? atau F1 | Bantuan |
| q atau Ctrl+C | Keluar |

Terminal dipulihkan otomatis saat keluar, termasuk setelah Ctrl+Z dan fg.
