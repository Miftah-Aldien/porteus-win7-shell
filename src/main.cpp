#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/keysym.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <string>
#include <vector>
#include <thread>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

namespace fs = std::filesystem;

static Display* dpy = nullptr;
static int screenNo = 0;
static Window rootWin = 0;
static Window desktopWin = 0;
static Window taskbarWin = 0;
static Window startMenuWin = 0;
static GC gc = 0;
static Atom wmDelete;
static int screenW = 1024, screenH = 600;
static const int BAR_H = 42;
static bool running = true;
static bool menuOpen = false;

struct FileView {
    Window win = 0;
    std::string path;
    std::vector<fs::directory_entry> entries;
    int scroll = 0;
};
static std::vector<FileView> fileViews;

static unsigned long rgb(unsigned char r, unsigned char g, unsigned char b) {
    return (static_cast<unsigned long>(r) << 16) |
           (static_cast<unsigned long>(g) << 8) | b;
}

static void setColor(unsigned long c) {
    XSetForeground(dpy, gc, c);
}

static void fill(Window w, int x, int y, unsigned int width, unsigned int height,
                 unsigned long color) {
    setColor(color);
    XFillRectangle(dpy, w, gc, x, y, width, height);
}

static void line(Window w, int x1, int y1, int x2, int y2, unsigned long color) {
    setColor(color);
    XDrawLine(dpy, w, gc, x1, y1, x2, y2);
}

static void txt(Window w, int x, int y, const std::string& s,
                unsigned long color = 0x202938) {
    setColor(color);
    XDrawString(dpy, w, gc, x, y, s.c_str(), static_cast<int>(s.size()));
}

static void setWindowType(Window w, const char* typeName) {
    Atom type = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE", False);
    Atom value = XInternAtom(dpy, typeName, False);
    XChangeProperty(dpy, w, type, XA_ATOM, 32, PropModeReplace,
                    reinterpret_cast<unsigned char*>(&value), 1);
}

static void launch(const std::string& command) {
    pid_t pid = fork();
    if (pid == 0) {
        setsid();
        execl("/bin/sh", "sh", "-c", command.c_str(), (char*)nullptr);
        _exit(127);
    }
}

static std::string homeDir() {
    const char* h = std::getenv("HOME");
    return h ? h : "/";
}

static void refreshEntries(FileView& v) {
    v.entries.clear();
    try {
        for (const auto& e : fs::directory_iterator(v.path)) v.entries.push_back(e);
        std::sort(v.entries.begin(), v.entries.end(),
            [](const fs::directory_entry& a, const fs::directory_entry& b) {
                bool ad = a.is_directory(), bd = b.is_directory();
                if (ad != bd) return ad > bd;
                return a.path().filename().string() < b.path().filename().string();
            });
    } catch (...) {}
}

static void drawFileView(FileView& v) {
    if (!v.win) return;
    XWindowAttributes a{};
    XGetWindowAttributes(dpy, v.win, &a);
    fill(v.win, 0, 0, a.width, a.height, 0xf4f6fa);
    fill(v.win, 0, 0, a.width, 34, 0xdce8f5);
    txt(v.win, 12, 22, "Address: " + v.path, 0x183451);
    line(v.win, 0, 35, a.width, 35, 0x9eb4cc);

    fill(v.win, 0, 36, 145, a.height - 36, 0xe7eef7);
    txt(v.win, 12, 60, "Favorites", 0x24496b);
    txt(v.win, 20, 84, "Desktop");
    txt(v.win, 20, 106, "Documents");
    txt(v.win, 20, 128, "Pictures");
    txt(v.win, 20, 150, "Computer");
    line(v.win, 145, 36, 145, a.height, 0xb8c8d8);

    txt(v.win, 164, 58, "Name", 0x24496b);
    line(v.win, 155, 65, a.width, 65, 0xb8c8d8);
    int y = 88;
    int shown = 0;
    for (int i = v.scroll; i < static_cast<int>(v.entries.size()) && shown < 22; ++i, ++shown) {
        const auto& e = v.entries[static_cast<size_t>(i)];
        std::string name = e.path().filename().string();
        if (name.size() > 48) name = name.substr(0, 45) + "...";
        std::string prefix = e.is_directory() ? "[Folder] " : "[File]   ";
        txt(v.win, 164, y, prefix + name, e.is_directory() ? 0x245c91 : 0x303840);
        y += 22;
    }
    XFlush(dpy);
}

static void openFileManager(const std::string& path) {
    FileView v;
    v.path = path.empty() ? homeDir() : path;
    v.win = XCreateSimpleWindow(dpy, rootWin, 120, 70, 720, 440, 1,
                                0x7892ad, 0xf4f6fa);
    XStoreName(dpy, v.win, "Computer - File Explorer");
    XSelectInput(dpy, v.win, ExposureMask | ButtonPressMask | KeyPressMask |
                              StructureNotifyMask);
    XSetWMProtocols(dpy, v.win, &wmDelete, 1);
    setWindowType(v.win, "_NET_WM_WINDOW_TYPE_NORMAL");
    refreshEntries(v);
    fileViews.push_back(v);
    XMapRaised(dpy, v.win);
    drawFileView(fileViews.back());
}

static void drawDesktop() {
    // A restrained blue gradient-like wallpaper made from horizontal bands.
    for (int y = 0; y < screenH - BAR_H; y += 4) {
        double t = static_cast<double>(y) / std::max(1, screenH - BAR_H);
        unsigned char r = static_cast<unsigned char>(10 + 30 * t);
        unsigned char g = static_cast<unsigned char>(55 + 75 * t);
        unsigned char b = static_cast<unsigned char>(105 + 90 * t);
        fill(desktopWin, 0, y, screenW, 4, rgb(r, g, b));
    }
    txt(desktopWin, 22, 26, "Computer", 0xffffff);
    txt(desktopWin, 22, 91, "Home", 0xffffff);
    txt(desktopWin, 22, 156, "File Explorer", 0xffffff);
    txt(desktopWin, 22, 221, "Trash", 0xffffff);
    // Simple desktop icon tiles.
    fill(desktopWin, 22, 36, 30, 27, 0xd7e9f7);
    fill(desktopWin, 22, 101, 30, 27, 0xd7e9f7);
    fill(desktopWin, 22, 166, 30, 27, 0xd7e9f7);
    fill(desktopWin, 22, 231, 30, 27, 0xd7e9f7);
    XFlush(dpy);
}

static void drawTaskbar() {
    fill(taskbarWin, 0, 0, screenW, BAR_H, 0x172b45);
    fill(taskbarWin, 0, 0, screenW, 2, 0x8bbce7);
    // Start orb approximation.
    fill(taskbarWin, 8, 6, 34, 30, 0x2876bd);
    txt(taskbarWin, 16, 27, "W", 0xffffff);
    line(taskbarWin, 48, 5, 48, BAR_H - 5, 0x45617d);

    // Show task buttons for other root-level windows.
    Window rr, parent, *children = nullptr;
    unsigned int n = 0;
    int x = 58;
    if (XQueryTree(dpy, rootWin, &rr, &parent, &children, &n)) {
        for (unsigned int i = 0; i < n && x < screenW - 145; ++i) {
            Window w = children[i];
            if (w == desktopWin || w == taskbarWin || w == startMenuWin) continue;
            bool ours = false;
            for (const auto& fv : fileViews) if (fv.win == w) ours = true;
            if (ours) continue;
            XWindowAttributes wa{};
            if (!XGetWindowAttributes(dpy, w, &wa) || wa.map_state != IsViewable) continue;
            char* name = nullptr;
            if (!XFetchName(dpy, w, &name) || !name) continue;
            std::string label(name);
            XFree(name);
            if (label.empty()) continue;
            if (label.size() > 13) label = label.substr(0, 12) + "~";
            fill(taskbarWin, x, 6, 132, 30, 0x294766);
            txt(taskbarWin, x + 8, 26, label, 0xffffff);
            x += 137;
        }
        if (children) XFree(children);
    }
    // Clock.
    char clockText[32];
    std::time_t now = std::time(nullptr);
    std::tm* tmNow = std::localtime(&now);
    if (tmNow) std::strftime(clockText, sizeof(clockText), "%H:%M", tmNow);
    else std::strcpy(clockText, "--:--");
    txt(taskbarWin, screenW - 65, 26, clockText, 0xffffff);
    XFlush(dpy);
}

static void drawStartMenu() {
    if (!menuOpen || !startMenuWin) return;
    fill(startMenuWin, 0, 0, 330, 390, 0xeaf1f8);
    fill(startMenuWin, 0, 0, 330, 54, 0x315d87);
    txt(startMenuWin, 18, 33, "Porteus Windows Desktop", 0xffffff);
    const char* items[] = {
        "Internet Browser", "File Explorer", "Terminal",
        "Documents", "Pictures", "Computer", "Control Panel",
        "Wine Applications", "Log out"
    };
    int y = 86;
    for (const char* item : items) {
        fill(startMenuWin, 12, y - 18, 30, 26, 0xc6d9eb);
        txt(startMenuWin, 52, y, item, 0x20364d);
        y += 34;
    }
    fill(startMenuWin, 0, 354, 330, 36, 0xd5e2ef);
    txt(startMenuWin, 18, 377, "Search programs and files", 0x65798d);
    XFlush(dpy);
}

static void createShellWindows() {
    desktopWin = XCreateSimpleWindow(dpy, rootWin, 0, 0, screenW, screenH, 0, 0, 0);
    XStoreName(dpy, desktopWin, "Win7Shell Desktop");
    setWindowType(desktopWin, "_NET_WM_WINDOW_TYPE_DESKTOP");
    XSelectInput(dpy, desktopWin, ExposureMask | ButtonPressMask);
    XMapWindow(dpy, desktopWin);
    XLowerWindow(dpy, desktopWin);

    taskbarWin = XCreateSimpleWindow(dpy, rootWin, 0, screenH - BAR_H,
                                     screenW, BAR_H, 0, 0, 0x172b45);
    XStoreName(dpy, taskbarWin, "Win7Shell Taskbar");
    setWindowType(taskbarWin, "_NET_WM_WINDOW_TYPE_DOCK");
    XSelectInput(dpy, taskbarWin, ExposureMask | ButtonPressMask);
    XMapRaised(dpy, taskbarWin);

    startMenuWin = XCreateSimpleWindow(dpy, rootWin, 8, screenH - BAR_H - 390,
                                       330, 390, 1, 0x8ba9c5, 0xeaf1f8);
    XStoreName(dpy, startMenuWin, "Win7Shell Start Menu");
    XSelectInput(dpy, startMenuWin, ExposureMask | ButtonPressMask);
    drawDesktop();
    drawTaskbar();
}

static void handleDesktopClick(XButtonEvent& e) {
    if (e.button != Button1) return;
    if (e.x < 100) {
        if (e.y < 80) launch("xdg-open computer:");
        else if (e.y < 145) openFileManager(homeDir());
        else if (e.y < 210) openFileManager(homeDir());
        else launch("xdg-open trash:");
    }
}

static void handleStartClick(XButtonEvent& e) {
    if (e.button != Button1) return;
    int y = e.y;
    if (y >= 354) return; // Search field is decorative in this first version.
    if (y < 54) return;
    int index = (y - 68) / 34;
    switch (index) {
        case 0: launch("xdg-open https://"); break;
        case 1: openFileManager(homeDir()); break;
        case 2: launch("x-terminal-emulator"); break;
        case 3: openFileManager(homeDir() + "/Documents"); break;
        case 4: openFileManager(homeDir() + "/Pictures"); break;
        case 5: openFileManager("/"); break;
        case 6: launch("wine control"); break;
        case 7: openFileManager(homeDir() + "/.wine/drive_c"); break;
        case 8: running = false; break;
        default: break;
    }
    menuOpen = false;
    XUnmapWindow(dpy, startMenuWin);
}

static void handleFileClick(FileView& v, XButtonEvent& e) {
    if (e.button != Button1) return;
    if (e.x < 145 && e.y > 65) {
        if (e.y < 96) v.path = homeDir();
        else if (e.y < 118) v.path = homeDir() + "/Documents";
        else if (e.y < 140) v.path = homeDir() + "/Pictures";
        else if (e.y < 162) v.path = "/";
        refreshEntries(v);
        drawFileView(v);
        return;
    }
    if (e.x < 150 || e.y < 68) return;
    int idx = v.scroll + (e.y - 77) / 22;
    if (idx < 0 || idx >= static_cast<int>(v.entries.size())) return;
    fs::path p = v.entries[static_cast<size_t>(idx)].path();
    if (fs::is_directory(p)) {
        v.path = p.string();
        v.scroll = 0;
        refreshEntries(v);
        drawFileView(v);
    } else {
        launch("xdg-open \"" + p.string() + "\"");
    }
}

static void cleanup() {
    if (dpy) {
        XCloseDisplay(dpy);
        dpy = nullptr;
    }
}

int main() {
    dpy = XOpenDisplay(nullptr);
    if (!dpy) {
        std::fprintf(stderr, "win7shell: cannot open X display. Is Xorg running?\n");
        return 1;
    }
    screenNo = DefaultScreen(dpy);
    rootWin = RootWindow(dpy, screenNo);
    screenW = DisplayWidth(dpy, screenNo);
    screenH = DisplayHeight(dpy, screenNo);
    if (screenW < 640 || screenH < 480) {
        std::fprintf(stderr, "win7shell: display is too small (%dx%d)\n", screenW, screenH);
        cleanup();
        return 1;
    }
    gc = XCreateGC(dpy, rootWin, 0, nullptr);
    wmDelete = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
    createShellWindows();

    auto lastRefresh = std::chrono::steady_clock::now();
    while (running) {
        while (XPending(dpy)) {
            XEvent ev;
            XNextEvent(dpy, &ev);
            if (ev.type == Expose) {
                if (ev.xexpose.window == desktopWin) drawDesktop();
                else if (ev.xexpose.window == taskbarWin) drawTaskbar();
                else if (ev.xexpose.window == startMenuWin) drawStartMenu();
                else for (auto& fv : fileViews)
                    if (ev.xexpose.window == fv.win) drawFileView(fv);
            } else if (ev.type == ButtonPress) {
                if (ev.xbutton.window == desktopWin) handleDesktopClick(ev.xbutton);
                else if (ev.xbutton.window == taskbarWin) {
                    if (ev.xbutton.x < 48) {
                        menuOpen = !menuOpen;
                        if (menuOpen) {
                            XMapRaised(dpy, startMenuWin);
                            drawStartMenu();
                        } else XUnmapWindow(dpy, startMenuWin);
                    }
                } else if (ev.xbutton.window == startMenuWin) handleStartClick(ev.xbutton);
                else for (auto& fv : fileViews)
                    if (ev.xbutton.window == fv.win) handleFileClick(fv, ev.xbutton);
            } else if (ev.type == KeyPress) {
                if (ev.xkey.keycode == XKeysymToKeycode(dpy, XK_Escape) && menuOpen) {
                    menuOpen = false;
                    XUnmapWindow(dpy, startMenuWin);
                }
            } else if (ev.type == ClientMessage) {
                if (static_cast<Atom>(ev.xclient.data.l[0]) == wmDelete) {
                    for (auto it = fileViews.begin(); it != fileViews.end(); ++it) {
                        if (it->win == ev.xclient.window) {
                            XDestroyWindow(dpy, it->win);
                            fileViews.erase(it);
                            break;
                        }
                    }
                }
            }
        }
        auto now = std::chrono::steady_clock::now();
        if (now - lastRefresh > std::chrono::seconds(2)) {
            drawTaskbar();
            lastRefresh = now;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    for (auto& fv : fileViews) XDestroyWindow(dpy, fv.win);
    if (startMenuWin) XDestroyWindow(dpy, startMenuWin);
    if (taskbarWin) XDestroyWindow(dpy, taskbarWin);
    if (desktopWin) XDestroyWindow(dpy, desktopWin);
    XFreeGC(dpy, gc);
    cleanup();
    return 0;
}
