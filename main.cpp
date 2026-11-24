// Porteus Win7-like Shell - v0.2 shell mode
// Native X11 shell for Porteus 5.01 i686 + Wine 8
// Design: Openbox = window manager, win7shell = desktop + taskbar + start menu
// Wine apps are normal X11 windows, tracked via _NET_CLIENT_LIST
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
static Display* dpy=nullptr;
static int screenNo=0;
static Window rootWin=0, desktopWin=0, taskbarWin=0, startMenuWin=0;
static GC gc=0;
static Atom wmDelete, netClientList, netWmName, netActiveWindow, netWmWindowType, netWmState;
static int screenW=1024, screenH=768;
static const int BAR_H=42;
static bool running=true, menuOpen=false;
struct FileView{ Window win=0; std::string path; std::vector<fs::directory_entry> entries; int scroll=0; };
static std::vector<FileView> fileViews;
static unsigned long rgb(unsigned char r,unsigned char g,unsigned char b){ return ( (unsigned long)r<<16)|((unsigned long)g<<8)|b; }
static void setColor(unsigned long c){ XSetForeground(dpy,gc,c); }
static void fill(Window w,int x,int y,unsigned int width,unsigned int height,unsigned long color){ setColor(color); XFillRectangle(dpy,w,gc,x,y,width,height); }
static void line(Window w,int x1,int y1,int x2,int y2,unsigned long color){ setColor(color); XDrawLine(dpy,w,gc,x1,y1,x2,y2); }
static void txt(Window w,int x,int y,const std::string& s,unsigned long color=0x202938){ setColor(color); XDrawString(dpy,w,gc,x,y,s.c_str(),(int)s.size()); }
static void setWindowType(Window w,const char* t){ Atom v=XInternAtom(dpy,t,False); XChangeProperty(dpy,w,netWmWindowType,XA_ATOM,32,PropModeReplace,(unsigned char*)&v,1); }
static void setState(Window w,const char* s){ Atom v=XInternAtom(dpy,s,False); XChangeProperty(dpy,w,netWmState,XA_ATOM,32,PropModeAppend,(unsigned char*)&v,1); }
static void launch(const std::string& cmd){ pid_t pid=fork(); if(pid==0){ setsid(); execl("/bin/sh","sh","-c",cmd.c_str(),(char*)nullptr); _exit(127);} }
static std::string homeDir(){ const char* h=getenv("HOME"); return h?h:"/"; }
static std::string lowerExt(const std::string& p){ auto pos=p.rfind('.'); if(pos==std::string::npos) return ""; std::string e=p.substr(pos); for(auto &c:e) c=tolower(c); return e; }
static void openWithWineOrXdg(const std::string& p){
  std::string e=lowerExt(p);
  if(e==".exe"||e==".msi"||e==".bat"||e==".com") launch("wine \""+p+"\" &");
  else if(fs::is_directory(p)) launch("xdg-open \""+p+"\" &");
  else launch("xdg-open \""+p+"\" &");
}
static void refreshEntries(FileView& v){ v.entries.clear(); try{ for(auto &e: fs::directory_iterator(v.path)) v.entries.push_back(e); std::sort(v.entries.begin(),v.entries.end(),[](auto &a,auto &b){ bool ad=a.is_directory(), bd=b.is_directory(); if(ad!=bd) return ad>bd; return a.path().filename().string()<b.path().filename().string(); }); }catch(...){} }
static void drawFileView(FileView& v){ if(!v.win) return; XWindowAttributes a{}; XGetWindowAttributes(dpy,v.win,&a); fill(v.win,0,0,a.width,a.height,0xf4f6fa); fill(v.win,0,0,a.width,34,0xdce8f5); txt(v.win,12,22,"Address: "+v.path,0x183451); line(v.win,0,35,a.width,35,0x9eb4cc); fill(v.win,0,36,145,a.height-36,0xe7eef7); txt(v.win,12,60,"Favorites",0x24496b); txt(v.win,20,84,"Desktop"); txt(v.win,20,106,"Documents"); txt(v.win,20,128,"Pictures"); txt(v.win,20,150,"Computer"); line(v.win,145,36,145,a.height,0xb8c8d8); txt(v.win,164,58,"Name",0x24496b); line(v.win,155,65,a.width,65,0xb8c8d8); int y=88, shown=0; for(int i=v.scroll;i<(int)v.entries.size()&&shown<22;++i,++shown){ auto &e=v.entries[(size_t)i]; std::string name=e.path().filename().string(); if(name.size()>48) name=name.substr(0,45)+"..."; txt(v.win,164,y,(e.is_directory()?"[Folder] ":"[File]   ")+name,e.is_directory()?0x245c91:0x303840); y+=22; } XFlush(dpy); }
static void openFileManager(const std::string& path){ FileView v; v.path=path.empty()?homeDir():path; v.win=XCreateSimpleWindow(dpy,rootWin,120,70,720,440,1,0x7892ad,0xf4f6fa); XStoreName(dpy,v.win,"Computer - File Explorer"); XSelectInput(dpy,v.win,ExposureMask|ButtonPressMask|KeyPressMask|StructureNotifyMask); XSetWMProtocols(dpy,v.win,&wmDelete,1); setWindowType(v.win,"_NET_WM_WINDOW_TYPE_NORMAL"); refreshEntries(v); fileViews.push_back(v); XMapRaised(dpy,v.win); drawFileView(fileViews.back()); }
static void drawDesktop(){ for(int y=0;y<screenH-BAR_H;y+=4){ double t=(double)y/std::max(1,screenH-BAR_H); fill(desktopWin,0,y,screenW,4,rgb(10+30*t,55+75*t,105+90*t)); } const char* labels[]={"Computer","Home","File Explorer","Trash"}; int yy=22; for(auto *l:labels){ fill(desktopWin,22,yy+14,30,27,0xd7e9f7); txt(desktopWin,22,yy,l,0xffffff); yy+=65; } XFlush(dpy); }
static std::vector<Window> clientList(){ std::vector<Window> out; Atom actual; int fmt; unsigned long n,bytes; unsigned char* data=nullptr; if(XGetWindowProperty(dpy,rootWin,netClientList,0,1024,False,XA_WINDOW,&actual,&fmt,&n,&bytes,&data)==Success && data){ Window* w=(Window*)data; for(unsigned long i=0;i<n;++i) out.push_back(w[i]); XFree(data);} return out; }
static std::string winName(Window w){ char* n=nullptr; std::string s; if(XFetchName(dpy,w,&n)&&n){ s=n; XFree(n);} return s; }
static bool isOurs(Window w){ if(w==desktopWin||w==taskbarWin||w==startMenuWin) return true; for(auto &fv:fileViews) if(fv.win==w) return true; return false; }
static void activate(Window w){ XRaiseWindow(dpy,w); XSetInputFocus(dpy,w,RevertToParent,CurrentTime); XChangeProperty(dpy,rootWin,netActiveWindow,XA_WINDOW,32,PropModeReplace,(unsigned char*)&w,1); XFlush(dpy); }
static void drawTaskbar(){ fill(taskbarWin,0,0,screenW,BAR_H,0x172b45); fill(taskbarWin,0,0,screenW,2,0x8bbce7); fill(taskbarWin,8,6,34,30,0x2876bd); txt(taskbarWin,16,27,"W",0xffffff); line(taskbarWin,48,5,48,BAR_H-5,0x45617d); int x=58; for(Window w: clientList()){ if(isOurs(w)) continue; XWindowAttributes wa{}; if(!XGetWindowAttributes(dpy,w,&wa)||wa.map_state!=IsViewable) continue; std::string label=winName(w); if(label.empty()) continue; if(label.size()>16) label=label.substr(0,15)+"~"; if(x>screenW-220) break; fill(taskbarWin,x,6,150,30,0x294766); txt(taskbarWin,x+8,26,label,0xffffff); x+=155; } char ct[32]; time_t now=time(nullptr); tm* t=localtime(&now); if(t) strftime(ct,sizeof(ct),"%H:%M",t); else strcpy(ct,"--:--"); txt(taskbarWin,screenW-65,26,ct,0xffffff); XFlush(dpy); }
// Start menu items -> real commands. Wine apps launched via wine, NOT wine explorer.
struct MenuItem{ const char* label; const char* cmd; };
static MenuItem menuItems[]={
 {"Internet Browser","xdg-open https://example.com &"},
 {"File Explorer",""},
 {"Terminal","xterm &"},
 {"Documents","xdg-open ~/Documents &"},
 {"Pictures","xdg-open ~/Pictures &"},
 {"Computer",""},
 {"Notepad (Wine)","wine notepad &"},
 {"Wine Control Panel","wine control &"},
 {"Log out","pkill -f win7shell; pkill openbox &"},
};
static void drawStartMenu(){ if(!menuOpen||!startMenuWin) return; fill(startMenuWin,0,0,330,430,0xeaf1f8); fill(startMenuWin,0,0,330,54,0x315d87); txt(startMenuWin,18,33,"Porteus Windows Desktop",0xffffff); int y=86; for(auto &it: menuItems){ fill(startMenuWin,12,y-18,30,26,0xc6d9eb); txt(startMenuWin,52,y,it.label,0x202938); y+=32; } txt(startMenuWin,18,410,"Search (visual only)",0x6b7a8d); XFlush(dpy); }
static void handleStartMenuClick(int y){ int idx=(y-68)/32; if(idx<0||idx>=(int)(sizeof(menuItems)/sizeof(menuItems[0]))) return; auto &it=menuItems[idx]; std::string l=it.label; menuOpen=false; XUnmapWindow(dpy,startMenuWin); if(l=="File Explorer"||l=="Computer") openFileManager(homeDir()); else if(it.cmd && it.cmd[0]) launch(it.cmd); drawTaskbar(); }
static void handleDesktopClick(int x,int y){ if(x>=22&&x<=52&&y>=36&&y<=63) openFileManager(homeDir()); else if(x>=22&&x<=52&&y>=101&&y<=128) openFileManager(homeDir()); }
static void handleTaskbarClick(int x){ if(x>=8&&x<=42){ menuOpen=!menuOpen; if(menuOpen){ XMapRaised(dpy,startMenuWin); drawStartMenu(); } else XUnmapWindow(dpy,startMenuWin); return; } int bx=58; for(Window w: clientList()){ if(isOurs(w)) continue; XWindowAttributes wa{}; if(!XGetWindowAttributes(dpy,w,&wa)||wa.map_state!=IsViewable) continue; if(winName(w).empty()) continue; if(x>=bx&&x<bx+150){ activate(w); return; } bx+=155; if(bx>screenW-220) break; } }
int main(){ dpy=XOpenDisplay(nullptr); if(!dpy){ fprintf(stderr,"No DISPLAY\n"); return 1; } screenNo=DefaultScreen(dpy); rootWin=DefaultRootWindow(dpy); screenW=DisplayWidth(dpy,screenNo); screenH=DisplayHeight(dpy,screenNo); gc=XCreateGC(dpy,rootWin,0,nullptr); wmDelete=XInternAtom(dpy,"WM_DELETE_WINDOW",False); netClientList=XInternAtom(dpy,"_NET_CLIENT_LIST",False); netWmName=XInternAtom(dpy,"_NET_WM_NAME",False); netActiveWindow=XInternAtom(dpy,"_NET_ACTIVE_WINDOW",False); netWmWindowType=XInternAtom(dpy,"_NET_WM_WINDOW_TYPE",False); netWmState=XInternAtom(dpy,"_NET_WM_STATE",False);
 // Desktop: bottom layer, sticky, never focus
 desktopWin=XCreateSimpleWindow(dpy,rootWin,0,0,screenW,screenH-BAR_H,0,0x0a3755,0x0a3755); setWindowType(desktopWin,"_NET_WM_WINDOW_TYPE_DESKTOP"); setState(desktopWin,"_NET_WM_STATE_BELOW"); setState(desktopWin,"_NET_WM_STATE_STICKY"); XStoreName(dpy,desktopWin,"Porteus Desktop"); XSelectInput(dpy,desktopWin,ExposureMask|ButtonPressMask); XMapWindow(dpy,desktopWin); XLowerWindow(dpy,desktopWin);
 // Taskbar: dock, always on top
 taskbarWin=XCreateSimpleWindow(dpy,rootWin,0,screenH-BAR_H,screenW,BAR_H,0,0x172b45,0x172b45); setWindowType(taskbarWin,"_NET_WM_WINDOW_TYPE_DOCK"); setState(taskbarWin,"_NET_WM_STATE_ABOVE"); setState(taskbarWin,"_NET_WM_STATE_STICKY"); XStoreName(dpy,taskbarWin,"Taskbar"); XSelectInput(dpy,taskbarWin,ExposureMask|ButtonPressMask); XMapWindow(dpy,taskbarWin);
 // Start menu
 startMenuWin=XCreateSimpleWindow(dpy,rootWin,0,screenH-BAR_H-430,330,430,1,0x315d87,0xeaf1f8); setWindowType(startMenuWin,"_NET_WM_WINDOW_TYPE_MENU"); XSelectInput(dpy,startMenuWin,ExposureMask|ButtonPressMask|KeyPressMask);
 drawDesktop(); drawTaskbar();
 XSelectInput(dpy,rootWin,SubstructureNotifyMask|PropertyChangeMask);
 auto last=std::chrono::steady_clock::now();
 while(running){ while(XPending(dpy)){ XEvent ev; XNextEvent(dpy,&ev); if(ev.type==Expose){ if(ev.xexpose.window==desktopWin) drawDesktop(); else if(ev.xexpose.window==taskbarWin) drawTaskbar(); else if(ev.xexpose.window==startMenuWin) drawStartMenu(); else for(auto &fv:fileViews) if(ev.xexpose.window==fv.win) drawFileView(fv); } else if(ev.type==ButtonPress){ Window w=ev.xbutton.window; int x=ev.xbutton.x, y=ev.xbutton.y; if(w==desktopWin) handleDesktopClick(x,y); else if(w==taskbarWin) handleTaskbarClick(x); else if(w==startMenuWin) handleStartMenuClick(y); else for(auto &fv:fileViews) if(w==fv.win){ if(ev.xbutton.button==1){ /* single click select, double-click open */ } if(ev.xbutton.button==1 && ev.xbutton.time%2==0){} } } else if(ev.type==KeyPress){ KeySym ks=XLookupKeysym(&ev.xkey,0); if(ks==XK_Escape && menuOpen){ menuOpen=false; XUnmapWindow(dpy,startMenuWin); } } else if(ev.type==ClientMessage){ if((Atom)ev.xclient.data.l[0]==wmDelete){ for(auto it=fileViews.begin();it!=fileViews.end();++it) if(it->win==ev.xclient.window){ XDestroyWindow(dpy,it->win); fileViews.erase(it); break; } } } else if(ev.type==DestroyNotify){ for(auto it=fileViews.begin();it!=fileViews.end();++it) if(it->win==ev.xdestroywindow.window){ fileViews.erase(it); break; } } }
  auto now=std::chrono::steady_clock::now(); if(std::chrono::duration_cast<std::chrono::milliseconds>(now-last).count()>500){ last=now; drawTaskbar(); }
  std::this_thread::sleep_for(std::chrono::milliseconds(20));
 }
 return 0;
}
