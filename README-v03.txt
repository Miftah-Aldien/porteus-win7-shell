v0.3 Taskbar Aero - 1024x600, transparan dikit ala Win7 default

1. Install picom di Porteus (modul picom) - kalau belum ada, taskbar tetap jalan tapi tanpa blur
2. Copy picom.conf ke ~/.config/picom.conf
3. Copy win7-session ke /usr/local/bin/win7-session, chmod +x
4. Jalankan win7-session atau ./win7shell + picom --config ~/.config/picom.conf -b &
5. Atur transparansi: di picom.conf, opacity-rule 92 = transparan dikit. 85 = lebih bening, 96 = lebih solid.

Yang baru di v0.3:
- Taskbar gradient kaca + highlight putih di atas
- Orb Start bulat 4 warna (bukan kotak + W lagi)
- Tombol task kaca + highlight
- Jam + Show Desktop di pojok kanan (klik Show Desktop = minimize semua, pakai xdotool, kalau xdotool belum ada tombol itu diam saja)
- picom.conf ringan: xrender, blur-strength 3, tanpa shadow berat (aman untuk netbook)

Belum di v0.3 (tahap berikutnya): Start Menu Aero, Desktop beneran, ikon PNG.
