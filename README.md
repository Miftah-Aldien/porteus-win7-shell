# Porteus Win7-like Shell (prototype)

A small native X11 desktop-shell prototype for Porteus 5.01 i686. It is **not**
Microsoft Windows Explorer and does not reproduce the proprietary Windows 7
shell. It provides a Windows-7-inspired desktop, Start menu, taskbar, and a
basic file browser, while Wine runs Windows applications.

## Important design notes

- The shell is a native Linux/X11 program, not a Windows `.exe`.
- Wine remains a separate compatibility layer for Windows applications.
- A lightweight X11 window manager (for example Openbox) is still recommended
  to move, resize, focus, and decorate ordinary application windows.
- LXQt can remain installed as a recovery desktop while testing.
- This first prototype uses Xlib and the C++17 standard library.
- File opening uses `xdg-open`; install/configure it or change the command in
  `src/main.cpp` to match your Porteus setup.
- The Start-menu search box is currently visual only. Taskbar window tracking
  and system tray integration are prototype-level and need later refinement.
- The visual style is inspired by Windows 7; it is not a pixel-perfect clone.

## Build on GitHub

1. Create a GitHub repository and upload these files.
2. Open **Actions** and enable workflows if GitHub asks.
3. Run **Build Porteus 32-bit** using `workflow_dispatch`, or push a commit.
4. Download artifact `win7shell-linux-i686`.

The workflow builds inside a 32-bit Debian Bullseye container, producing an ELF i386 binary against an older glibc baseline than current Ubuntu runners. This improves compatibility with older 32-bit Linux distributions, but the binary must still be checked on the exact Porteus installation.

## Runtime dependencies on Porteus

- Xorg/X11 server
- 32-bit X11 runtime library (`libX11.so.6`)
- A running X11 session (`DISPLAY` must be set)
- Optional: Openbox or another lightweight X11 window manager
- Optional: `xdg-open` for opening files and URLs

Check the downloaded binary:

```sh
file win7shell
ldd win7shell
```

If `ldd` reports a missing library, install/activate the matching Porteus
module before running the shell.

## First test (keep LXQt available)

Open a terminal inside LXQt:

```sh
chmod +x ./win7shell
./win7shell
```

The prototype creates a desktop surface and bottom taskbar. Click the blue
"W" button to open the Start menu. Click the File Explorer entry to open the
file browser. Press Escape to close the Start menu.

Stop it with `Ctrl+C` in the terminal. Do not configure it as the boot shell
until the interactive test succeeds.

## Suggested next stage

For a standalone session, start Xorg with a minimal session script that runs
a window manager and then `win7shell`, rather than starting LXQt. Keep a
separate recovery entry that starts LXQt until the shell is stable.
