# Porteus Win7-like Shell v0.2 (shell mode)

Shell desktop Win7 untuk Porteus 5.01 i686 + Wine 8.

Arsitektur:
  Boot -> Xorg -> Openbox (WM) -> win7shell (desktop+taskbar+start menu) -> Wine apps (jendela X biasa)

Bukan LXQt. LXQt hanya cadangan saat testing.

## File
- src/main.cpp = shell v0.2 (desktop layer bawah, taskbar lacak _NET_CLIENT_LIST, Start Menu launch wine)
- win7-session = script sesi boot, taruh di /usr/local/bin/win7-session, chmod +x
- openbox-rc-snippet.txt = setting Openbox
- .github/workflows/build-i686.yml = build cross-compile -m32, tanpa QEMU (cepat, timeout 15 menit)

## Build di GitHub
Push -> Actions -> download artifact win7shell-linux-i686

## Test aman
1. Dari LXQt: ./win7shell, lalu dari terminal lain: wine notepad
   Cek Notepad muncul di taskbar.
2. Kalau stabil: jalankan win7-session sebagai sesi baru.
3. Kalau sudah stabil 2-3x boot: jadikan default, LXQt tetap cadangan.

## Catatan v0.2
- Resolusi dibaca dari X11, bukan hardcode 1024x600
- Desktop: _NET_WM_WINDOW_TYPE_DESKTOP + BELOW + STICKY
- Taskbar: _NET_WM_WINDOW_TYPE_DOCK + ABOVE + STICKY
- Start Menu: wine notepad, wine control, xterm, xdg-open
- File Explorer: .exe/.msi -> wine, lainnya -> xdg-open
- Belum: system tray Wine, minimize/maximize penuh dari taskbar, search Start Menu
