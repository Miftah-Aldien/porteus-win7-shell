v5.1 - KOREKSI PENTING: Foto Win7 hanya referensi TAMPILAN, bukan isi.

TAMPILAN dari foto: Start Menu 2 kolom, kiri putih, kanan gelap, user miftah, Search, Shut down, taskbar Aero, orb, klik kanan persis foto.
ISI dari Wine: Start Menu kiri dibaca dari ~/.local/share/applications/wine/Programs/*.desktop + Start Menu Programs di Wine.
  - Kalau Wine ada app A, B, C -> muncul A, B, C. Bukan dipaksa jadi Word/Chess/Millionaire dari foto.
  - Kalau Wine kosong -> fallback NETRAL: Notepad (Wine), Paint (Wine), File Explorer, Terminal, Wine Control Panel. Bukan daftar foto.
  - Install app Wine baru -> otomatis muncul tanpa build ulang.

Langsung build v5.1 ini, tidak perlu v4.
