// Porteus Win7-like Shell - v0.3 Taskbar Aero
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
static bool desktopShown=false;
static std::vector<Window> minimizedWins;
static Window popupWin=0;
static std::vector<std::string> popupItems;
static std::string popupContext; // taskbar, taskbutton, startbutton
static Window popupTarget=0;
static void hidePopup(){ if(popupWin){ XUnmapWindow(dpy,popupWin); } }
static void showPopup(int x,int y, std::vector<std::string> items, std::string ctx, Window target=0){
  popupItems=items; popupContext=ctx; popupTarget=target;
  if(!popupWin){ popupWin=XCreateSimpleWindow(dpy,rootWin,x,y,220, (int)items.size()*28+8, 1, 0x6a8ab0, 0xf0f4f8); XSelectInput(dpy,popupWin,ExposureMask|ButtonPressMask); }
  XMoveResizeWindow(dpy,popupWin, std::max(0,std::min(x, screenW-230)), std::max(0, y - (int)items.size()*28 - 10), 220, (int)items.size()*28+8);
  XMapRaised(dpy,popupWin);
  // draw
  for(size_t i=0;i<items.size();++i){ txt(popupWin,12, (int)i*28+20, items[i], 0x1a2a3a); if(i+1<items.size()) line(popupWin,8,(int)i*28+28,212,(int)i*28+28,0xc8d4de); }
  XFlush(dpy);
}
static void doShowDesktopToggle(){
  if(!desktopShown){
    minimizedWins.clear();
    for(Window w: clientList()){ if(isOurs(w)) continue; XWindowAttributes wa{}; if(!XGetWindowAttributes(dpy,w,&wa)||wa.map_state!=IsViewable) continue; minimizedWins.push_back(w); XIconifyWindow(dpy,w,screenNo); }
    desktopShown=true;
  } else {
    for(Window w: minimizedWins){ XMapWindow(dpy,w); XRaiseWindow(dpy,w); }
    minimizedWins.clear(); desktopShown=false;
  }
  XFlush(dpy);
}

struct FileView{ Window win=0; std::string path; std::vector<fs::directory_entry> entries; int scroll=0; };
static std::vector<FileView> fileViews;
static unsigned long rgb(unsigned char r,unsigned char g,unsigned char b){ return ( (unsigned long)r<<16)|((unsigned long)g<<8)|b; }
static void setColor(unsigned long c){ XSetForeground(dpy,gc,c); }
static void fill(Window w,int x,int y,unsigned int width,unsigned int height,unsigned long color){ setColor(color); XFillRectangle(dpy,w,gc,x,y,width,height); }
static void line(Window w,int x1,int y1,int x2,int y2,unsigned long color){ setColor(color); XDrawLine(dpy,w,gc,x1,y1,x2,y2); }
static void txt(Window w,int x,int y,const std::string& s,unsigned long color=0x202938){ setColor(color); XDrawString(dpy,w,gc,x,y,s.c_str(),(int)s.size()); }
static void setWindowType(Window w,const char* t){ Atom v=XInternAtom(dpy,t,False); XChangeProperty(dpy,w,netWmWindowType,XA_ATOM,32,PropModeReplace,(unsigned char*)&v,1); }
static void setState(Window w,const char* s){ Atom v=XInternAtom(dpy,s,False); XChangeProperty(dpy,w,netWmState,XA_ATOM,32,PropModeAppend,(unsigned char*)&v,1); }
static Window createOverrideWindow(int x,int y,unsigned int w,unsigned int h,unsigned long border,unsigned long bg){
  XSetWindowAttributes attr{}; attr.override_redirect=True; attr.background_pixel=bg; attr.border_pixel=border;
  Window win=XCreateWindow(dpy,rootWin,x,y,w,h,0,CopyFromParent,InputOutput,CopyFromParent,CWOverrideRedirect|CWBackPixel|CWBorderPixel,&attr);
  return win;
}
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
static void drawTaskbar(){
  // === TASKBAR AERO v0.3 - 1024x600, transparan dikit (ala Win7 default) ===
  // Triknya: bukan alpha beneran di Xlib (itu butuh ARGB visual + picom),
  // tapi glass gradient + highlight + picom opacity 0.92 di picom.conf
  // Hasil: wallpaper di belakang masih samar, taskbar tetap kebaca.
  // Gradasi vertikal: atas lebih terang (kaca), bawah lebih gelap
  for(int y=0;y<BAR_H;++y){
    double t=(double)y/(double)BAR_H;
    unsigned char r=(unsigned char)(18+22*t);
    unsigned char g=(unsigned char)(38+28*t);
    unsigned char b=(unsigned char)(62+28*t);
    fill(taskbarWin,0,y,screenW,1,rgb(r,g,b));
  }
  // Highlight kaca di atas (garis putih samar)
  fill(taskbarWin,0,0,screenW,1,0x9fc4e8);
  fill(taskbarWin,0,1,screenW,1,0x5a8ab8);
  // Garis bawah tipis
  fill(taskbarWin,0,BAR_H-1,screenW,1,0x0a1a2c);

  // --- ORB START (bulat, 4 warna, ganti kotak + W) ---
  // Posisi: x=6, y=3, diameter 36 (pas untuk BAR_H=42)
  // Lingkaran luar gelap, dalam 4 kuadran warna Win7
  // Merah kanan-atas, Hijau kiri-atas, Biru kanan-bawah, Kuning kiri-bawah
  // Kita gambar dengan FillArc sederhana + highlight
  int ox=6, oy=3, od=36;
  setColor(0x0a2038); XFillArc(dpy,taskbarWin,gc,ox,oy,od,od,0,360*64);
  // Kuadran - pakai FillArc 90 derajat per warna
  setColor(0xe74c3c); XFillArc(dpy,taskbarWin,gc,ox+2,oy+2,od-4,od-4,0,90*64);      // merah
  setColor(0x2ecc71); XFillArc(dpy,taskbarWin,gc,ox+2,oy+2,od-4,od-4,90*64,90*64);   // hijau
  setColor(0x3498db); XFillArc(dpy,taskbarWin,gc,ox+2,oy+2,od-4,od-4,180*64,90*64); // biru
  setColor(0xf1c40f); XFillArc(dpy,taskbarWin,gc,ox+2,oy+2,od-4,od-4,270*64,90*64); // kuning
  // Highlight kaca di orb (setengah atas lebih terang)
  setColor(0xffffff); XDrawArc(dpy,taskbarWin,gc,ox+4,oy+3,od-8,od-8,30*64,120*64);
  // Pemisah setelah orb
  line(taskbarWin,50,5,50,BAR_H-5,0x3a5a7a);

  // --- TOMBOL TASK (kaca, glow kalau aktif) ---
  int x=58;
  for(Window w: clientList()){
    if(isOurs(w)) continue;
    XWindowAttributes wa{}; if(!XGetWindowAttributes(dpy,w,&wa)||wa.map_state!=IsViewable) continue;
    std::string label=winName(w); if(label.empty()) continue;
    if(label.size()>14) label=label.substr(0,13)+"~";
    if(x>screenW-240) break;
    // Tombol: gradient kaca, border terang di atas
    for(int yy=0;yy<30;++yy){ double tt=(double)yy/30.0; fill(taskbarWin,x,6+yy,150,1,rgb(35+25*tt,62+25*tt,92+20*tt)); }
    fill(taskbarWin,x,6,150,1,0x9fc4e8); // highlight atas tombol
    // Ikon task v0.5: tidak lagi kotak putih polos.
    // Warna + huruf per aplikasi (sambil nunggu _NET_WM_ICON asli di v0.6)
    // Notepad = biru + N, Explorer = kuning + E, Terminal = hitam + >_, Wine Control = ungu + C, lain = abu + huruf pertama
    unsigned long iconBg=0x7a9ab0; std::string iconLetter="?";
    std::string low=label; for(auto &c: low) c=tolower(c);
    if(low.find("notepad")!=std::string::npos){ iconBg=0x2f7ac0; iconLetter="N"; }
    else if(low.find("explorer")!=std::string::npos||low.find("computer")!=std::string::npos){ iconBg=0xe8b93a; iconLetter="E"; }
    else if(low.find("terminal")!=std::string::npos||low.find("shell")!=std::string::npos){ iconBg=0x1a1a1a; iconLetter=">"; }
    else if(low.find("control")!=std::string::npos){ iconBg=0x7a4ac0; iconLetter="C"; }
    else if(low.find("wine")!=std::string::npos){ iconBg=0x8a1a2a; iconLetter="W"; }
    else if(!label.empty()){ iconLetter=label.substr(0,1); }
    fill(taskbarWin,x+6,12,18,18,iconBg);
    txt(taskbarWin,x+11,25,iconLetter,0xffffff);
    txt(taskbarWin,x+30,26,label,0xffffff);
    x+=155;
  }

  // --- JAM + SHOW DESKTOP (kanan) ---
  // Area jam: sedikit lebih gelap + pemisah
  line(taskbarWin,screenW-118,5,screenW-118,BAR_H-5,0x3a5a7a);
  char ct[32]; time_t now=time(nullptr); tm* t=localtime(&now);
  if(t) strftime(ct,sizeof(ct),"%H:%M",t); else strcpy(ct,"--:--");
  txt(taskbarWin,screenW-105,26,ct,0xffffff);
  // Show Desktop: kotak kecil transparan di pojok paling kanan (ala Win7)
  // x = screenW-18, lebar 16
  fill(taskbarWin,screenW-18,4,16,BAR_H-8,0x2a4a6a);
  line(taskbarWin,screenW-18,4,screenW-18,BAR_H-4,0x7aa8cc);
  XFlush(dpy);
}
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
struct WineApp{ std::string name; std::string execCmd; };
static std::vector<WineApp> wineApps;
static bool wineAppsLoaded=false;
static std::string trimStr(std::string s){ while(!s.empty() && (s.back()=='\r'||s.back()=='\n'||s.back()==' ')) s.pop_back(); while(!s.empty() && s.front()==' ') s.erase(s.begin()); return s; }
static void loadWineApps(){
  if(wineAppsLoaded) return; wineAppsLoaded=true; wineApps.clear();
  std::string home=homeDir();
  std::vector<std::string> roots={ home+"/.local/share/applications/wine/Programs", home+"/.wine/drive_c/ProgramData/Microsoft/Windows/Start Menu/Programs" };
  for(auto &root: roots){
    try{
      if(!fs::exists(root)) continue;
      for(auto &e: fs::recursive_directory_iterator(root)){
        if(e.path().extension()!=".desktop") continue;
        std::string fname=e.path().filename().string(); std::string low=fname; for(auto &c: low) c=tolower(c);
        if(low.find("uninstall")!=std::string::npos||low.find("uninstal")!=std::string::npos) continue;
        if(low.find("help")!=std::string::npos) continue;
        std::string name="", execCmd="";
        FILE* f=fopen(e.path().c_str(),"r"); if(!f) continue; char buf[1024];
        while(fgets(buf,sizeof(buf),f)){ std::string line=trimStr(buf); if(line.rfind("Name=",0)==0) name=line.substr(5); else if(line.rfind("Exec=",0)==0) execCmd=line.substr(5); if(!execCmd.empty()){ size_t pos=execCmd.find('%'); if(pos!=std::string::npos) execCmd=execCmd.substr(0,pos); execCmd=trimStr(execCmd); } }
        fclose(f); if(name.empty()||execCmd.empty()) continue;
        bool dup=false; for(auto &a: wineApps) if(a.name==name) dup=true; if(!dup) wineApps.push_back({name,execCmd});
      }
    }catch(...){}
  }
  std::sort(wineApps.begin(), wineApps.end(), [](auto &a, auto &b){ return a.name<b.name; });
}
static void drawStartMenu(){
  if(!menuOpen||!startMenuWin) return;
  // Start Menu Win7 asli (foto 1): lebar 2 kolom, bukan 1 kolom seperti v0.3
  // Kiri: putih, daftar program + Search. Kanan: gelap transparan, Places + Shut down
  // Ukuran di 1024x600: total 500x430, kiri 280, kanan 220
  fill(startMenuWin,0,0,500,430,0xeaf1f8);
  // Kolom kanan gelap (di foto: hitam-biru gelap)
  fill(startMenuWin,280,0,220,430,0x1a2f4a);
  fill(startMenuWin,280,0,220,430,0x223a5a);
  // Header kanan: user miftah + foto
  fill(startMenuWin,295,12,38,38,0xc6d9eb); txt(startMenuWin,340,38,"miftah",0xffffff);
  // Kiri: daftar program (dari foto)
    loadWineApps();
  std::vector<std::string> leftNames;
  for(auto &a: wineApps){ if((int)leftNames.size()>=11) break; leftNames.push_back(a.name); }
  if(leftNames.empty()){
    // FOTO HANYA REFERENSI TAMPILAN. Isi = dari Wine. Kalau Wine kosong, tampil netral saja, JANGAN paksa daftar foto.
    const char* fallback[]={"Notepad (Wine)","Paint (Wine)","File Explorer","Terminal","Wine Control Panel"};
    for(auto *f: fallback) leftNames.push_back(f);
  }
  int y=12; for(auto &itStr: leftNames){ const char* it = itStr.c_str(); unsigned long bg=0x4a7ab0; if(std::string(it).find("Paint")!=std::string::npos) bg=0xc05a2a; else if(std::string(it).find("Word")!=std::string::npos) bg=0x2a5ac0; else if(std::string(it).find("Excel")!=std::string::npos) bg=0x2a8a4a; else if(std::string(it).find("VLC")!=std::string::npos) bg=0xe88a2a; else if(std::string(it).find("Notepad")!=std::string::npos) bg=0x2f7ac0; fill(startMenuWin,12,y,28,24,bg); std::string L=std::string(it).substr(0,1); txt(startMenuWin,21,y+17,L,0xffffff); txt(startMenuWin,48,y+16,it,0x1a2a3a); y+=28; }
  txt(startMenuWin,48,y+16,"All Programs  >",0x1a2a3a);
  // Search box kiri bawah
  fill(startMenuWin,12,388,256,26,0xffffff); txt(startMenuWin,18,405,"Search programs and files",0x8a9aaa);
  // Kanan: Places (dari foto)
  const char* rightItems[]={"Documents","Pictures","Music","Games","Computer","Control Panel","Devices and Printers","Default Programs","Run..."};
  int ry=68; for(auto *it: rightItems){ txt(startMenuWin,295,ry,it,0xffffff); ry+=28; }
  // Shut down kanan bawah
  fill(startMenuWin,295,388,130,26,0x3a6a9a); txt(startMenuWin,305,405,"Shut down  >",0xffffff);
  XFlush(dpy);
}
static void handleStartMenuClick(int x,int y){
  menuOpen=false; XUnmapWindow(dpy,startMenuWin); loadWineApps();
  if(x<280){
    if(y>=388 && y<=414) return;
    int idx=(y-12)/28; if(idx<0) return;
    if(!wineApps.empty()){
      if(idx < (int)wineApps.size() && idx<11){ launch(wineApps[idx].execCmd + " &"); return; }
    } else {
      const char* fallbackCmds[]={"wine notepad &","wine mspaint &","xdg-open ~/ &","xterm &","wine control &"};
      if(idx<5) launch(fallbackCmds[idx]);
      return;
    }
    if(idx==11){ openFileManager(homeDir()+"/.local/share/applications/wine/Programs"); return; }
  } else {
    if(y>=388 && y<=414){ launch("pkill -f win7shell; pkill openbox &"); return; }
    if(y>=12 && y<=50){ openFileManager(homeDir()); return; }
    int ridx=(y-68)/28; const char* places[]={"Documents","Pictures","Music","Games","Computer","Control Panel","Devices and Printers","Default Programs","Run..."};
    if(ridx<0||ridx>=9) return; std::string pl=places[ridx];
    if(pl=="Documents") launch("xdg-open ~/Documents &");
    else if(pl=="Pictures") launch("xdg-open ~/Pictures &");
    else if(pl=="Music") launch("xdg-open ~/Music &");
    else if(pl=="Games") openFileManager(homeDir()+"/.local/share/applications/wine/Programs");
    else if(pl=="Computer") openFileManager(homeDir());
    else if(pl=="Control Panel") launch("wine control &");
    else if(pl=="Devices and Printers") launch("wine control printers &");
    else if(pl=="Default Programs") launch("wine control &");
    else if(pl=="Run...") launch("xterm &");
  }
  drawTaskbar();
}
static Window thumbWin=0;
static void showThumbnailPreview(int x, Window w){
  // Foto 4: hover tombol task -> muncul jendela kaca di atas taskbar berisi thumbnail
  // Di v0.4 ini thumbnail digambar sebagai kotak preview + nama jendela (bukan screenshot asli jendela,
  // karena screenshot asli butuh XComposite - tahap berikutnya)
  if(!thumbWin){ thumbWin=XCreateSimpleWindow(dpy,rootWin,0,0,220,150,1,0x6a8ab0,0xeaf1f8); XSelectInput(dpy,thumbWin,ExposureMask); }
  std::string name = winName(w); if(name.empty()) name="Window";
  int tx = std::max(0, std::min(x, screenW-230));
  XMoveResizeWindow(dpy,thumbWin,tx, screenH-BAR_H-160, 220, 150);
  XMapRaised(dpy,thumbWin);
  fill(thumbWin,0,0,220,150,0x2a4a6a); fill(thumbWin,4,24,212,122,0xf4f6fa);
  txt(thumbWin,10,16,name,0xffffff); txt(thumbWin,20,60,"Preview - "+name,0x1a2a3a); txt(thumbWin,20,84,"(Aero Peek - v0.4)",0x6a8ab0);
  XFlush(dpy);
}
static void hideThumbnail(){ if(thumbWin) XUnmapWindow(dpy,thumbWin); }
static void handlePopupClick(int y){
  int idx = (y-8)/28;
  if(idx<0||idx>=(int)popupItems.size()){ hidePopup(); return; }
  std::string item = popupItems[idx];
  hidePopup();
  if(popupContext=="taskbar"){
    if(item=="Show the desktop"||item=="Show Desktop") doShowDesktopToggle();
    else if(item=="Start Task Manager"||item=="Open Task Manager") launch("xterm -e top &");
    else if(item=="Cascade windows"||item=="Show windows stacked"||item=="Show windows side by side") { /* v0.4: tile jendela, placeholder - tidak crash */ }
    else if(item=="Lock the taskbar  [v]"||item=="Lock the Taskbar") { /* dikunci */ }
    else if(item=="Properties") launch("xterm -e \"echo Taskbar Properties - v0.4; read\" &");
  } else if(popupContext=="taskbutton"){
    if(!popupTarget) return;
    if(item=="Close all windows") XDestroyWindow(dpy,popupTarget);
    else if(item=="Unpin this program from taskbar") { /* unpin: hapus dari pinned, placeholder */ }
    else if(item=="Windows Explorer") openFileManager(homeDir());
    else if(item=="Minimize") XIconifyWindow(dpy,popupTarget,screenNo);
    else if(item=="Restore"||item=="Minimize") activate(popupTarget);
    else if(item=="Close") XDestroyWindow(dpy,popupTarget);
  } else if(popupContext=="startbutton"){
    if(item=="Open Windows Explorer"||item=="Open"||item=="Explore") openFileManager(homeDir());
    else if(item=="Properties") launch("xterm -e \"echo Start Menu Properties - v0.4; read\" &");
  }
}
int main(){ dpy=XOpenDisplay(nullptr); if(!dpy){ fprintf(stderr,"No DISPLAY\n"); return 1; } screenNo=DefaultScreen(dpy); rootWin=DefaultRootWindow(dpy); screenW=DisplayWidth(dpy,screenNo); screenH=DisplayHeight(dpy,screenNo); gc=XCreateGC(dpy,rootWin,0,nullptr); wmDelete=XInternAtom(dpy,"WM_DELETE_WINDOW",False); netClientList=XInternAtom(dpy,"_NET_CLIENT_LIST",False); netWmName=XInternAtom(dpy,"_NET_WM_NAME",False); netActiveWindow=XInternAtom(dpy,"_NET_ACTIVE_WINDOW",False); netWmWindowType=XInternAtom(dpy,"_NET_WM_WINDOW_TYPE",False); netWmState=XInternAtom(dpy,"_NET_WM_STATE",False);
 // Desktop: bottom layer, sticky, never focus
 desktopWin=createOverrideWindow(0,0,screenW,screenH-BAR_H,0x0a3755,0x0a3755); setWindowType(desktopWin,"_NET_WM_WINDOW_TYPE_DESKTOP"); setState(desktopWin,"_NET_WM_STATE_BELOW"); setState(desktopWin,"_NET_WM_STATE_STICKY"); XStoreName(dpy,desktopWin,"Porteus Desktop"); XSelectInput(dpy,desktopWin,ExposureMask|ButtonPressMask); XMapWindow(dpy,desktopWin); XLowerWindow(dpy,desktopWin);
 // Taskbar: dock, always on top
 taskbarWin=createOverrideWindow(0,screenH-BAR_H,screenW,BAR_H,0x172b45,0x172b45); setWindowType(taskbarWin,"_NET_WM_WINDOW_TYPE_DOCK"); setState(taskbarWin,"_NET_WM_STATE_ABOVE"); setState(taskbarWin,"_NET_WM_STATE_STICKY"); XStoreName(dpy,taskbarWin,"Taskbar"); XSelectInput(dpy,taskbarWin,ExposureMask|ButtonPressMask); XMapWindow(dpy,taskbarWin);
 // Start menu
 startMenuWin=createOverrideWindow(0,screenH-BAR_H-430,500,430,0x315d87,0xeaf1f8); setWindowType(startMenuWin,"_NET_WM_WINDOW_TYPE_MENU"); XSelectInput(dpy,startMenuWin,ExposureMask|ButtonPressMask|KeyPressMask);
 XMoveWindow(dpy,desktopWin,0,0); XMoveWindow(dpy,taskbarWin,0,screenH-BAR_H); XMoveResizeWindow(dpy,startMenuWin,0,screenH-BAR_H-430,500,430); drawDesktop(); drawTaskbar();
 XSelectInput(dpy,rootWin,SubstructureNotifyMask|PropertyChangeMask);
 auto last=std::chrono::steady_clock::now();
 while(running){ while(XPending(dpy)){ XEvent ev; XNextEvent(dpy,&ev); if(ev.type==Expose){ if(ev.xexpose.window==desktopWin) drawDesktop(); else if(ev.xexpose.window==taskbarWin) drawTaskbar(); else if(ev.xexpose.window==startMenuWin) drawStartMenu(); else for(auto &fv:fileViews) if(ev.xexpose.window==fv.win) drawFileView(fv); } else if(ev.type==ButtonPress){ Window w=ev.xbutton.window; int x=ev.xbutton.x, y=ev.xbutton.y; if(w==desktopWin){ hidePopup(); handleDesktopClick(x,y); } else if(w==taskbarWin) handleTaskbarClick(x, ev.xbutton.button); else if(w==popupWin) handlePopupClick(y); else if(w==startMenuWin) handleStartMenuClick(x,y); else for(auto &fv:fileViews) if(w==fv.win){ if(ev.xbutton.button==1){ /* single click select, double-click open */ } if(ev.xbutton.button==1 && ev.xbutton.time%2==0){} } } else if(ev.type==KeyPress){ KeySym ks=XLookupKeysym(&ev.xkey,0); if(ks==XK_Escape && menuOpen){ menuOpen=false; XUnmapWindow(dpy,startMenuWin); } } else if(ev.type==ClientMessage){ if((Atom)ev.xclient.data.l[0]==wmDelete){ for(auto it=fileViews.begin();it!=fileViews.end();++it) if(it->win==ev.xclient.window){ XDestroyWindow(dpy,it->win); fileViews.erase(it); break; } } } else if(ev.type==DestroyNotify){ for(auto it=fileViews.begin();it!=fileViews.end();++it) if(it->win==ev.xdestroywindow.window){ fileViews.erase(it); break; } } }
  auto now=std::chrono::steady_clock::now(); if(std::chrono::duration_cast<std::chrono::milliseconds>(now-last).count()>500){ last=now; drawTaskbar(); }
  std::this_thread::sleep_for(std::chrono::milliseconds(20));
 }
 return 0;
}
