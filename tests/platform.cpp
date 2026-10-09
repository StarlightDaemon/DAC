#include "nativewake-input-fixture.h"
#define DAC_HARNESS
#include "../src/dac.cpp"
#undef GetRawInputData
#undef GetMessageTime
#undef GetMessagePos
#undef GetCursorPos
#undef MonitorFromPoint
#undef GetForegroundWindow
#undef IsWindowVisible
#undef GetMessageW
#include <cstdlib>
#include <psapi.h>
using namespace dac;
void Check(bool ok,const char* what) { if(!ok) { fprintf(stderr,"FAIL %s error=%lu\n",what,GetLastError()); fflush(stdout);fflush(stderr);std::_Exit(1); } }
std::wstring Executable() { wchar_t exe[32768]; DWORD n=GetModuleFileNameW(nullptr,exe,32768); Check(n&&n<32768,"exe path"); return std::wstring(exe,n); }
void Pump() { MSG msg;while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)) {TranslateMessage(&msg);DispatchMessageW(&msg);} }
struct SessionSnapshot {size_t runs{},windows{},jobs{},powers{};int powerPhase=0;State a{},b{},c{};Time aGeneration{},bGeneration{};bool hidden=true;};
SessionSnapshot snapshot;
LRESULT SetupSessions() {
    injectedObservations=true;controller=Controller{};controller.config.perInput=true;controller.config.media=false;
    controller.config.monitors={{"A",{true,1}},{"B",{true,2}},{"C",{true,-1}}};
    displays={{"A","",L"Injected A",nullptr,{-640,0,0,360},true},{"B","",L"Injected B",nullptr,{0,0,640,360},true},{"C","",L"Injected C",nullptr,{640,0,1280,360},true}};
    fixtureModes={{"A",L"--preview-responsive"},{"B",L"--preview-responsive"}};
    controller.Topology({"A","B","C"},GetTickCount64());InvalidateMedia();return 1;
}
LRESULT StartSessions() {ManualAll();Reconcile();return 1;}
LRESULT SnapshotSessions() {
    Reconcile();snapshot={};snapshot.runs=runs.size();
    snapshot.powers=powerTasks.size();if(!powerTasks.empty())snapshot.powerPhase=powerTasks.front()->status;
    {std::lock_guard lock(registryLock);snapshot.windows=ownedWindows.size();snapshot.jobs=ownedJobs.size();}
    snapshot.a=controller.nodes["A"].state;snapshot.b=controller.nodes["B"].state;snapshot.c=controller.nodes["C"].state;
    snapshot.aGeneration=controller.nodes["A"].generation;snapshot.bGeneration=controller.nodes["B"].generation;
    for(auto& r:runs) if(r->window&&IsWindowVisible(r->window)) snapshot.hidden=false;
    return 1;
}
LRESULT DismissA() {
    auto now=GetTickCount64();
    controller.Input(now,{},false); // timer fallback before raw input
    controller.Activity(now,false,"A",{"B"},true);
    controller.Input(now,{},false); // fallback after raw input
    controller.Activity(now,false,"A",{"B"},false); // delayed queue observation
    Reconcile();return 1;
}
LRESULT StopSessions() {Reset();Reconcile();return 1;}
LRESULT HungSession() {fixtureModes["B"]=L"--preview-hung";controller.Manual("B",GetTickCount64());Reconcile();return 1;}
LRESULT PauseSessions() {controller.paused=true;controller.Tick(GetTickCount64(),{});Reconcile();return 1;}
LRESULT ResumeSessions() {controller.paused=false;Reset();return 1;}
LRESULT BlockSessions() {locked=true;SetBlocked();Reconcile();return 1;}
LRESULT UnblockSessions() {locked=false;SetBlocked();return 1;}
LRESULT RemoveA() {controller.Topology({"B","C"},GetTickCount64());displays.erase(displays.begin());Reconcile();return 1;}
LRESULT ChangeSettings() {Config c=controller.config;for(auto& [id,p]:c.monitors){(void)id;p.enabled=false;}return Save(c);}
LRESULT ConfigureSession() {fixtureModes["A"]=L"--hang";return AddRun(displays[0],0,1,true)&&!AddRun(displays[0],0,1,true);}
void OnUi(LRESULT(*action)()) {DWORD_PTR result=0;Check(SendMessageTimeoutW(ui,WM_APP+101,0,reinterpret_cast<LPARAM>(action),SMTO_ABORTIFHUNG,2000,&result)&&result,"UI-owner production integration command");}
template<class Predicate> void Until(Predicate predicate,DWORD timeout,const char* what) {
    Time begin=GetTickCount64();do {OnUi(SnapshotSessions);if(predicate())return;Sleep(20);}while(GetTickCount64()-begin<timeout);Check(false,what);
}
void Sessions(bool soak) {
    CloseHandle(stopEvent);stopEvent=nullptr;Check(Initialize(),"integration initialize");OnUi(SetupSessions);OnUi(StartSessions);
    Until([]{return snapshot.runs==3&&snapshot.a==State::Saver&&snapshot.b==State::Saver&&snapshot.c==State::Black;},4000,"two saver sessions plus black through controller/reconciler");
    Check(snapshot.hidden&&snapshot.jobs==2&&snapshot.windows==3,"injected presentations hidden and independently owned");
    OnUi(DismissA);Until([]{return snapshot.runs==2;},3000,"independent A cleanup");
    Check(snapshot.b==State::Saver&&snapshot.c==State::Black,"B and C survive input on A");OnUi(StopSessions);Until([]{return snapshot.runs==0;},3000,"all session cleanup");
    if(!soak) {
        OnUi(HungSession);Until([]{return snapshot.b==State::Saver;},3000,"fixture becomes responsive before hanging");
        Until([]{return snapshot.b==State::Fallback&&snapshot.jobs==0;},6500,"hung-after-start fallback and owned cleanup");
        auto generation=snapshot.bGeneration;Sleep(200);OnUi(SnapshotSessions);Check(snapshot.bGeneration==generation&&snapshot.runs==1,"fallback keeps black without retry");
        OnUi(StopSessions);Until([]{return snapshot.runs==0;},3000,"fallback stop");
        OnUi(SetupSessions);OnUi(StartSessions);OnUi(PauseSessions);Until([]{return snapshot.runs==0;},3000,"pause cleanup");
        OnUi(ResumeSessions);OnUi(StartSessions);OnUi(BlockSessions);Until([]{return snapshot.runs==0;},3000,"lock/power gate cleanup");
        OnUi(UnblockSessions);OnUi(StartSessions);OnUi(ChangeSettings);Until([]{return snapshot.runs==0;},3000,"saved settings invalidate active sessions");
        OnUi(SetupSessions);OnUi(ConfigureSession);OnUi(StopSessions);Until([]{return snapshot.runs==0;},3000,"configuration job duplicate rejection/stop");
        OnUi(SetupSessions);OnUi(StartSessions);OnUi(RemoveA);Until([]{return snapshot.runs==0;},3000,"topology invalidates all old generations");
        puts("HARNESS sessions PASS: controller->reconciler->windows/workers; concurrent independent hidden fixtures; hung health; settings/pause/session/topology cleanup");
    } else {
        DWORD handles0=0,handles1=0;PROCESS_MEMORY_COUNTERS_EX before{},after{};
        GetProcessHandleCount(GetCurrentProcess(),&handles0);GetProcessMemoryInfo(GetCurrentProcess(),reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&before),sizeof(before));
        Time began=GetTickCount64();
        for(int i=0;i<100;++i) {
            OnUi(StartSessions);Until([]{return snapshot.a==State::Saver&&snapshot.b==State::Saver;},3000,"soak startup");
            OnUi(StopSessions);Until([]{return snapshot.runs==0;},3000,"soak cleanup");
            if(i%20==19){printf("HARNESS presentation cycle %d/100\n",i+1);fflush(stdout);}
        }
        GetProcessHandleCount(GetCurrentProcess(),&handles1);GetProcessMemoryInfo(GetCurrentProcess(),reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&after),sizeof(after));
        printf("HARNESS presentation soak: 100 cycles, %llu ms; handles %lu -> %lu; private bytes %zu -> %zu (hidden fixture processes, not GPU/display soak)\n",GetTickCount64()-began,handles0,handles1,static_cast<size_t>(before.PrivateUsage),static_cast<size_t>(after.PrivateUsage));
        Check(handles1<=handles0+4&&snapshot.jobs==0&&snapshot.windows==0,"no retained jobs/windows/handle growth after presentation soak");
    }
    Shutdown();injectedObservations=false;Check(runs.empty()&&ownedJobs.empty()&&ownedWindows.empty(),"integration final empty ownership");
}
void WorkerFixture(const wchar_t* args,bool healthy=false) {
    Run run; run.path=Executable();run.args=args;
    std::thread worker(RunWorker,&run);
    Time began=GetTickCount64();
    while(!run.done&&GetTickCount64()-began<8000) Sleep(20);
    if(!run.done) {run.cancel=true;SetEvent(stopEvent);}
    worker.join();Check(run.done&&run.status==(healthy?1:2),"production worker failure fallback");
    Check(ownedJobs.empty(),"production worker unregisters job");
    printf("PLATFORM worker %ls elapsed=%llu ms\n",args,GetTickCount64()-began);
}
void CaptureHidden(HWND window,const wchar_t* suffix) {
    RECT rect{};GetClientRect(window,&rect);HDC source=GetDC(window),memory=CreateCompatibleDC(source);HBITMAP bitmap=CreateCompatibleBitmap(source,rect.right,rect.bottom);auto old=SelectObject(memory,bitmap);
    // Compose the actual native paint paths offscreen. PrintWindow alone can
    // return success with black pixels on a noninteractive/hidden desktop.
    SendMessageW(window,WM_PRINTCLIENT,reinterpret_cast<WPARAM>(memory),PRF_CLIENT);
    for(HWND child=GetWindow(window,GW_CHILD);child;child=GetWindow(child,GW_HWNDNEXT)){
        RECT r{};GetWindowRect(child,&r);MapWindowPoints(nullptr,window,reinterpret_cast<POINT*>(&r),2);
        int saved=SaveDC(memory);IntersectClipRect(memory,r.left,r.top,r.right,r.bottom);SetViewportOrgEx(memory,r.left,r.top,nullptr);
        wchar_t cls[32]{};GetClassNameW(child,cls,32);bool custom=GetPropW(child,L"DacOriginalProc")&&!theme.highContrast;
        SendMessageW(child,custom?WM_PRINTCLIENT:WM_PRINT,reinterpret_cast<WPARAM>(memory),PRF_CLIENT|PRF_NONCLIENT|PRF_ERASEBKGND);
        RestoreDC(memory,saved);
    }
    Check(GetPixel(memory,0,0)==theme.colors.base,"native offscreen theme background pixel");
    UINT count=0,bytes=0;Gdiplus::GetImageEncodersSize(&count,&bytes);std::vector<BYTE> buffer(bytes);auto codecs=reinterpret_cast<Gdiplus::ImageCodecInfo*>(buffer.data());Gdiplus::GetImageEncoders(count,bytes,codecs);
    CLSID encoder{};bool found=false;for(UINT i=0;i<count;++i)if(wcscmp(codecs[i].MimeType,L"image/png")==0){encoder=codecs[i].Clsid;found=true;}Check(found,"PNG encoder");
    {Gdiplus::Bitmap image(bitmap,nullptr);Check(image.Save((Executable()+suffix).c_str(),&encoder,nullptr)==Gdiplus::Ok,"settings preview saved");}
    if(wcscmp(suffix,L".fujin-dark.png")==0){
        HICON preview=DacIcon(128,true);Check(preview!=nullptr,"scaled icon renders");
        {Gdiplus::Bitmap image(preview);Check(image.Save((Executable()+L".icon.png").c_str(),&encoder,nullptr)==Gdiplus::Ok,"icon preview saved");}DestroyIcon(preview);
    }
    SelectObject(memory,old);DeleteObject(bitmap);DeleteDC(memory);ReleaseDC(window,source);
}
void ExerciseSettingsDpi(HWND window,int editId,SettingsLayout& layout,int& scroll) {
    auto field=GetDlgItem(window,editId);SetDlgItemTextW(window,editId,L"777");SendMessageW(field,EM_SETSEL,1,2);
    SetWindowPos(window,nullptr,0,0,700,400,SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);
    SendMessageW(window,WM_VSCROLL,SB_PAGEDOWN,0);Check(scroll>0,"settings fixture scrolled before monitor transition");
    for(int cycle=0;cycle<3;++cycle)for(int dpi:{144,192,120,96}) {
        // Keep requested size within Windows' maximum tracking dimensions on
        // small CI desktops. Negative placement and every DPI are still tested.
        int width=std::min(MulDiv(700,dpi,96),std::max(300,GetSystemMetrics(SM_CXMAXTRACK)-32));
        // Quick setup explicitly requires 620 logical pixels; Windows honors
        // that minimum even when a synthetic DPI exceeds this desktop's scale.
        if(window==setupWindow)width=std::max(width,MulDiv(620,dpi,96));
        RECT suggested{-1500,70,-1500+width,470};
        auto previousFont=reinterpret_cast<HFONT>(SendMessageW(field,WM_GETFONT,0,0));
        SendMessageW(window,WM_DPICHANGED,MAKEWPARAM(dpi,dpi),reinterpret_cast<LPARAM>(&suggested));
        auto currentFont=reinterpret_cast<HFONT>(SendMessageW(field,WM_GETFONT,0,0));
        Check(currentFont&&currentFont!=previousFont&&GetObjectType(currentFont)==OBJ_FONT&&GetObjectType(previousFont)!=OBJ_FONT,"DPI transition releases replaced font and installs live replacement");
        Check(IsWindow(window)&&GetDlgItem(window,editId)==field,"DPI transition preserves window and control handles");
        Check(ControlText(window,editId)==L"777"&&layout.dpi==dpi,"DPI transition preserves unsaved text and applies new scale");
        DWORD start=0,end=0;SendMessageW(field,EM_GETSEL,reinterpret_cast<WPARAM>(&start),reinterpret_cast<LPARAM>(&end));Check(start==1&&end==2,"DPI transition preserves edit selection");
        RECT actual{};GetWindowRect(window,&actual);
        if(actual.left!=suggested.left||actual.top!=suggested.top||actual.right!=suggested.right)fprintf(stderr,"DPI %d requested %ld,%ld,%ld,%ld actual %ld,%ld,%ld,%ld\n",dpi,suggested.left,suggested.top,suggested.right,suggested.bottom,actual.left,actual.top,actual.right,actual.bottom);
        Check(actual.left==suggested.left&&actual.top==suggested.top&&actual.right==suggested.right,"DPI suggested position honored including negative coordinates");
        SCROLLINFO info{};info.cbSize=sizeof(info);info.fMask=SIF_ALL;GetScrollInfo(window,SB_VERT,&info);Check(scroll==info.nPos&&scroll>0,"DPI transition retains valid scroll offset");
        for(auto& c:layout.controls){RECT rect{};GetWindowRect(c.window,&rect);MapWindowPoints(nullptr,window,reinterpret_cast<POINT*>(&rect),2);
            Check(rect.left==MulDiv(c.x,dpi,96)&&rect.top==MulDiv(c.y,layout.dpi,96)-scroll&&rect.right-rect.left==MulDiv(c.width,dpi,96),"controls reflow from original logical coordinates without drift");}
    }
    // Process-wide GDI counts can change as other UI/COM workers initialize.
    // Assert each owned font's lifetime above; lifecycle covers aggregate leaks.
}
LRESULT AdvancedUi() {
    ShowSettings();Check(editor&&GetDlgItem(editor,149)&&GetDlgItem(editor,144),"advanced monitor controls created");
    auto edit=GetDlgItem(editor,101),checkbox=GetDlgItem(editor,146),combo=GetDlgItem(editor,122);
    SetWindowTextW(edit,L"777");CheckDlgButton(editor,146,BST_CHECKED);auto choice=SendMessageW(combo,CB_GETCURSEL,0,0);
    DWORD resources=GetGuiResources(GetCurrentProcess(),GR_GDIOBJECTS);
    for(int pass=0;pass<4;++pass)for(int mode:{0,1,2}){
        testTheme=mode;RefreshSettingsTheme();
        Check(edit==GetDlgItem(editor,101)&&ControlText(editor,101)==L"777"&&IsDlgButtonChecked(editor,146)==BST_CHECKED&&SendMessageW(combo,CB_GETCURSEL,0,0)==choice,"theme transitions retain controls and drafts");
        Check(theme.highContrast==(mode==2),"high contrast overrides Fujin");
        HDC dc=GetDC(edit);auto brush=SendMessageW(editor,WM_CTLCOLOREDIT,reinterpret_cast<WPARAM>(dc),reinterpret_cast<LPARAM>(edit));
        if(mode!=2)Check(brush==reinterpret_cast<LRESULT>(theme.surface)&&GetTextColor(dc)==theme.colors.text&&GetBkColor(dc)==theme.colors.surface,"native edit receives Fujin text and surface colors");ReleaseDC(edit,dc);
        LOGFONTW font{};GetObjectW(editorFont,sizeof(font),&font);if(mode!=2)Check(wcscmp(font.lfFaceName,fujin::fontFamily)==0,"Fujin native font");
        if(pass==0)CaptureHidden(editor,mode==0?L".fujin-light.png":mode==1?L".fujin-dark.png":L".high-contrast.png");
        if(pass==0&&mode==2)resources=GetGuiResources(GetCurrentProcess(),GR_GDIOBJECTS); // warm native theme/font/codec caches
    }
    Check(GetGuiResources(GetCurrentProcess(),GR_GDIOBJECTS)<=resources,"theme refresh has no GDI growth");
    testTheme=1;RefreshSettingsTheme();
    SendMessageW(checkbox,BM_CLICK,0,0);Check(IsDlgButtonChecked(editor,146)==BST_UNCHECKED,"themed checkbox keeps native toggle behavior");
    Check(SendMessageW(combo,CB_GETCOUNT,0,0)==11,"themed combo retains all monitor saver options");
    ICONINFO icon{};Check(theme.smallIcon&&theme.largeIcon&&theme.activeIcon&&GetIconInfo(theme.smallIcon,&icon),"custom tray and window icons exist");DeleteObject(icon.hbmMask);DeleteObject(icon.hbmColor);
    CaptureHidden(editor,L".settings.png");
    CheckDlgButton(editor,146,BST_CHECKED);auto selected=selection;ExerciseSettingsDpi(editor,101,editorLayout,editorScroll);
    Check(editor&&selection==selected&&IsDlgButtonChecked(editor,146)==BST_CHECKED,"main monitor draft survives move");
    ShowSaverOptions();Check(options&&GetDlgItem(options,201)&&GetDlgItem(options,220),"saver options created");
    ExerciseSettingsDpi(options,205,optionsLayout,optionsScroll);Check(editor&&!IsWindowEnabled(editor),"moving options retains modal ownership");
    CaptureHidden(options,L".options.png");
    Check(SendDlgItemMessageW(options,206,CB_GETCOUNT,0,0)==6,"six slideshow placements");
    SetDlgItemInt(options,205,11,FALSE);SendMessageW(options,WM_COMMAND,220,0);Check(!options&&draft.monitors[displays[selection].id].slideSeconds==11,"options apply to draft");
    DestroyWindow(editor);testTheme=-1;RefreshSettingsTheme();Check(!optionsFont&&!editorFont,"advanced UI font cleanup");return 1;
}
LRESULT SpanSession(){return Span(-1);}
LRESULT VerifySpan(){return runs.size()==1&&runs.front()->id=="@span"&&spanningDisplay.rect.left==-640&&spanningDisplay.rect.right==1280;}
LRESULT RejectSpan(){controller.config.monitors["C"].enabled=false;bool rejected=!Span(-1);controller.config.monitors["C"].enabled=true;return rejected;}
LRESULT ConfigurationFailure(){fixtureModes["A"]=L"--early";return AddRun(displays[0],0,1,true);}
LRESULT VerifyConfigurationFailure(){Reconcile();return configurationFailures==1;}
void Advanced() {
    CloseHandle(stopEvent);stopEvent=nullptr;trayFailures=1;Check(Initialize(),"tray unavailable is recoverable startup");
    OnUi(+[]()->LRESULT{
        Check(!trayPresent&&controller.blocked,"injected tray failure blocks controller");
        printf("ADVANCED initial session flags: locked=%d suspended=%d displayOff=%d; injecting awake session for tray recovery fixture\n",locked,suspended,displayOff);
        // Only the harness fixture is awake; the real desktop/session is untouched.
        locked=suspended=displayOff=false;trayFailures=0;Tray();return 1;
    });Time trayDeadline=GetTickCount64()+2000;while(!trayPresent&&GetTickCount64()<trayDeadline)Sleep(10);
    OnUi(+[]()->LRESULT{Check(trayPresent&&!controller.blocked,"asynchronous tray recovery unblocks");return 1;});OnUi(SetupSessions);OnUi(AdvancedUi);
    OnUi(SpanSession);Until([]{return snapshot.runs==1;},2000,"single desktop-spanning host");OnUi(VerifySpan);
    OnUi(DismissA);Until([]{return snapshot.runs==0;},2000,"span dismisses on activity anywhere");OnUi(RejectSpan);
    OnUi(ConfigurationFailure);Until([]{return snapshot.runs==0;},3000,"configuration failure cleaned");OnUi(VerifyConfigurationFailure);
    OnUi(+[]()->LRESULT{controller.config.hotkeys=true;RegisterControls();SendMessageW(ui,WM_HOTKEY,11,0);return controller.Any()&&controller.nodes["A"].sticky;});
    OnUi(+[]()->LRESULT{SendMessageW(ui,WM_HOTKEY,11,0);return !controller.Any();});Until([]{return snapshot.runs==0;},3000,"toggle-all cleanup");
    Shutdown();injectedObservations=false;puts("PLATFORM advanced PASS: Fujin light/dark/high-contrast painting and draft retention, native controls, icon creation, DPI/resource stability, recoverable tray startup, options/draft, span bounds/disabled exclusion, configuration error, sticky hotkey dispatch; no real hotkey registration or tray visibility claim");
}
void PowerTests() {
    configPath=Executable()+L".power-tests";const std::string id="injected-power-target";const auto path=PowerMarker(id);DeleteFileW(Extended(path).c_str());
    std::string token(32,'a');Check(PowerTicket(id+"\n"+token+"\nprobe",id,token,false)&&!PowerTicket(id+"\n"+token+"\noff",id,token,false),"off ticket single-use phase");
    Check(!PowerTicket(id+"\n"+token+"\nchanging",id,std::string(32,'b'),true)&&PowerTicket(id+"\n"+token+"\noff",id,token,true),"wake ticket operation binding");
    auto run=[&](int result,bool earlyCancel,bool wakeFail){
        fakePowerResult=result;fakePowerWakeResult=wakeFail?12:0;fakePowerDelay=earlyCancel?100:0;fakePowerOff=0;fakePowerOn=0;
        PowerTask task;task.id=id;std::thread worker(PowerWorker,&task);Time began=GetTickCount64();
        if(earlyCancel){Sleep(20);task.cancel=true;}else{while(!task.done&&task.status==0&&GetTickCount64()-began<2000)Sleep(1);task.cancel=true;}
        worker.join();Check(task.done,"power worker completed");return int(task.status.load());
    };
    Check(run(0,false,false)==4&&fakePowerOff==1&&fakePowerOn==1&&!PowerPending(id),"accepted off is woken once on cancellation");
    Check(run(0,true,false)==2&&fakePowerOff==1&&fakePowerOn==0&&!PowerPending(id),"cancellation during query prevents off and unnecessary wake");
    Check(run(11,false,false)==2&&fakePowerOn==0&&!PowerPending(id),"unsupported read never triggers wake");
    Check(run(12,false,false)==4&&fakePowerOn==1&&!PowerPending(id),"ambiguous failed write receives recovery wake");
    Check(run(0,false,true)==3&&fakePowerOn==1&&PowerPending(id),"failed wake retains quarantine");
    Check(run(0,false,false)==2&&fakePowerOff==0&&PowerPending(id),"quarantine blocks later off without DDC probe");
    DeleteFileW(Extended(path).c_str());controller=Controller{};Display display{id,"",L"Power fixture",nullptr,{0,0,100,100},true};
    fakePowerProcess=1;Check(run(0,false,false)==4&&!PowerPending(id),"real child transport observes off, signals cancellation and verifies awake acknowledgement");
    fakePowerProcess=2;Check(run(0,false,false)==3&&PowerPending(id),"zero process exit without acknowledgement is never success");DeleteFileW(Extended(path).c_str());fakePowerProcess=0;
    Check(StartPower(display,1)==0,"hardware disabled by default");controller.config.monitors[id].hardware=true;fakePowerDelay=100;
    Check(StartPower(display,1)==1&&StartPower(display,2)==-1,"one per-target worker; busy distinguished from rejection");StopPower();for(auto& task:powerTasks)task->worker.join();powerTasks.clear();DeleteFileW(Extended(path).c_str());
    fakePowerDelay=0;fakePowerWakeResult=0;puts("PLATFORM power PASS: simulated DDC only; operation tickets, off/wake/cancel, unsupported and ambiguous responses, wake quarantine, default opt-out and serialized generations");
}
std::wstring PhotoFixture() {
    auto folder=Executable()+L".photos";Check(CreateDirectoryW(folder.c_str(),nullptr)||GetLastError()==ERROR_ALREADY_EXISTS,"photo fixture directory");
    UINT count=0,bytes=0;Gdiplus::GetImageEncodersSize(&count,&bytes);std::vector<BYTE> memory(bytes);auto codecs=reinterpret_cast<Gdiplus::ImageCodecInfo*>(memory.data());Check(Gdiplus::GetImageEncoders(count,bytes,codecs)==Gdiplus::Ok,"image encoders");
    CLSID encoder{};bool found=false;for(UINT i=0;i<count;++i)if(wcscmp(codecs[i].MimeType,L"image/bmp")==0){encoder=codecs[i].Clsid;found=true;}Check(found,"BMP encoder");
    Gdiplus::Bitmap image(20,10,PixelFormat32bppARGB);Gdiplus::Graphics graphics(&image);graphics.Clear(Gdiplus::Color(255,255,0,0));
    Check(image.Save((folder+L"\\one.bmp").c_str(),&encoder,nullptr)==Gdiplus::Ok,"photo fixture save");return folder;
}
void Slides() {
    CloseHandle(stopEvent);stopEvent=nullptr;Check(Initialize(),"slideshow initialize");OnUi(SetupSessions);
    auto folder=PhotoFixture();Preference p;p.background=0x0000ff;
    for(int placement=0;placement<6;++placement){p.placement=placement;auto frame=SlideFrame(folder+L"\\one.bmp",100,100,p);Check(frame&&frame->GetWidth()==100&&frame->GetHeight()==100,"all placement frames rendered");
        Gdiplus::Color pixel;frame->GetPixel(50,50,&pixel);Check(pixel.GetR()>200,"photo remains centered or tiled");if(placement==0){frame->GetPixel(0,0,&pixel);Check(pixel.GetB()>200,"fit letterbox uses selected background");}}
    std::wstring corrupt=folder+L"\\bad.png";DWORD error=0;Check(WriteFileText(corrupt,"not an image",error),"corrupt fixture");Check(!SlideFrame(corrupt,100,100,p),"malformed image rejected");
    OnUi(+[]()->LRESULT{fixtureModes.clear();auto folder=Executable()+L".photos";for(auto id:{"A","B"}){auto& p=controller.config.monitors[id];p.saver=7;p.folder=Utf8(folder);p.slideSeconds=5;}controller.effectiveReady=false;controller.SelectPolicy(GetTickCount64(),0);ManualAll();return 1;});
    Until([]{return snapshot.a==State::Saver&&snapshot.b==State::Saver&&snapshot.runs==3;},4000,"two independent native slideshows plus black");Check(snapshot.jobs==0,"slideshow owns no saver process");
    OnUi(DismissA);Until([]{return snapshot.runs==2;},2000,"photo A independent dismissal");Check(snapshot.b==State::Saver,"photo B continues");OnUi(StopSessions);Until([]{return snapshot.runs==0;},2000,"photo frames and windows released");
    OnUi(+[]()->LRESULT{controller.config.monitors["A"].folder="C:\\dac-nonexistent-photo-fixture";controller.effectiveReady=false;controller.SelectPolicy(GetTickCount64(),0);controller.Manual("A",GetTickCount64());Reconcile();return 1;});
    Until([]{return snapshot.a==State::Fallback;},2500,"empty/missing folder uses black fallback");Shutdown();injectedObservations=false;
    puts("PLATFORM slideshow PASS: rendered six placements/background, malformed image, concurrent independent native frames, dismissal, missing-folder black fallback; hidden windows only");
}

std::array<XINPUT_STATE,4> injectedPads{};std::array<bool,4> injectedConnected{};unsigned padCalls=0;
DWORD WINAPI InjectedXInput(DWORD slot,XINPUT_STATE* state){++padCalls;if(!injectedConnected[slot])return ERROR_DEVICE_NOT_CONNECTED;*state=injectedPads[slot];return ERROR_SUCCESS;}
void ControllerAdapterTests(){XInputAdapter adapter;adapter.get=InjectedXInput;AdapterSnapshot sample;injectedConnected[0]=true;injectedPads[0].Gamepad.wButtons=XINPUT_GAMEPAD_A;
    adapter.Sample(0,true,sample);Check(sample.active&&sample.connected,"XInput initial held control");sample.active=false;adapter.Sample(250,true,sample);Check(sample.active&&sample.inputAt==250,"unchanged packet held control continues activity");injectedPads[0].Gamepad.wButtons=0;adapter.Sample(500,true,sample);Check(sample.active,"XInput release counts once");adapter.Sample(750,true,sample);Check(!sample.active,"neutral does not sustain activity");
    injectedPads[0].dwPacketNumber++;injectedPads[0].Gamepad.sThumbLX=100;adapter.Sample(1000,true,sample);Check(!sample.active,"noisy changed packet inside deadzone ignored");injectedConnected[0]=false;adapter.Sample(1250,true,sample);Check(!sample.connected&&!sample.active,"unplug clears held state");auto count=padCalls;adapter.Sample(1500,true,sample);Check(padCalls==count,"disconnected polling bounded");injectedConnected[0]=true;injectedPads[0].Gamepad={};adapter.Sample(3250,true,sample);Check(sample.connected&&!sample.active,"neutral reconnect is not activity");adapter.get=nullptr;adapter.Sample(3500,true,sample);Check(!sample.controllerAvailable&&!sample.active,"missing optional API defined inactive");}
LRESULT NextMigration(){auto savedPath=configPath;configPath=Executable()+L".nextbeta-settings";auto backup=configPath+L".v2-backup";DeleteFileW(Extended(backup).c_str());DWORD error=0;
    std::string original="\xef\xbb\xbf# reordered schema two fixture\ntimeout=17\nversion=2\nautomatic=0\nmonitor.41=0,2\npolicy.41=0,1,-1,0,0,0,0\nassets.41=||30|0|0|0|0\n",read;
    auto marker=PowerMarker("fixture-private-identity");std::string recovery="fixture-private-identity\nPRIVATE-RECOVERY-SECRET\nfault";Check(WriteFileText(marker,recovery,error)&&WriteFileText(configPath,original,error),"isolated schema2/recovery fixtures");Config next;next.monitors["A"].enabled=false;
    Check(WriteFileText(backup,"junk",error)&&!Save(next),"invalid preexisting rollback backup blocks migration");Check(ReadFileText(configPath,read,error,true)&&read==original,"rejected backup preserves exact source including BOM");DeleteFileW(Extended(backup).c_str());
    Check(Save(next),"first schema2 to schema3 transaction");Check(ReadFileText(backup,read,error,true)&&read==original,"migration backup byte exact");Config parsed;std::string parseError;Check(ReadFileText(configPath,read,error)&&Parse(read,parsed,parseError)&&parsed.sourceSchema==3&&!parsed.monitors["A"].enabled,"forward migration and disabled monitor survive");
    auto live=Serialize(controller.config);{Handle held(CreateFileW(Extended(configPath).c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,0,nullptr));Check(held&&!Save(Config{})&&Serialize(controller.config)==live,"failed replacement keeps active preferences");}
    Check(WriteFileText(configPath,"version=999\n",error)&&!Save(next),"future schema ordinary save fails closed");Check(ReadFileText(configPath,read,error)&&read=="version=999\n","future source preserved");Check(Save(Config{},true,true),"explicit reset can preserve and replace unsupported bytes");
    bool resetBackup=false;WIN32_FIND_DATAW item{};HANDLE find=FindFirstFileW(Extended(configPath+L".reset-backup-*").c_str(),&item);if(find!=INVALID_HANDLE_VALUE){auto dir=configPath.substr(0,configPath.find_last_of(L'\\')+1);do{std::string bytes;if(ReadFileText(dir+item.cFileName,bytes,error,true)&&bytes=="version=999\n")resetBackup=true;}while(FindNextFileW(find,&item));FindClose(find);}Check(resetBackup,"reset backup retains exact invalid original");
    Check(CopyFileW(Extended(backup).c_str(),Extended(configPath).c_str(),FALSE)&&ReadFileText(configPath,read,error,true)&&read==original,"documented rollback copies exact schema2 backup");Check(ReadFileText(configPath,read,error)&&Parse(read,parsed,parseError)&&parsed.sourceSchema==2&&parsed.monitors["A"].input==1&&!parsed.monitors["A"].enabled,"rollback schema2 parser fixture");Check(ReadFileText(marker,read,error)&&read==recovery,"migration reset rollback leave recovery bytes untouched");
    Policy imported;Config portable=Portable(parsed);Check(!portable.automatic&&!portable.monitors["A"].hardware,"portable excludes automatic and hardware grants");Check(!MapImported(portable,{},imported)&&MapImported(portable,{{"A","mapped-disconnected"}},imported)&&!imported.monitors["mapped-disconnected"].enabled&&!imported.monitors["mapped-disconnected"].hardware,"explicit remap required, disabled/disconnected retained");
    configPath=savedPath;return 1;
}
LRESULT NextUi(){ShowSettings();Check(editor&&GetDlgItem(editor,150)&&GetDlgItem(editor,160),"new stage/preview controls");SetDlgItemInt(editor,150,42,FALSE);SetDlgItemInt(editor,152,7,FALSE);SendDlgItemMessageW(editor,122,CB_SETCURSEL,9,0);StoreMonitorDraft();auto selected=editorDisplays[selection].id;
    ShowWorkspace();Check(workspace&&GetDlgItem(workspace,340)&&ControlText(workspace,303)==L"09:00","plain-language rules and HH:MM schedule controls");SetDlgItemTextW(workspace,343,L"C:\\trusted\\game.exe");SendMessageW(workspace,WM_COMMAND,323,0);Check(workspaceRules.size()==1&&workspaceRules[0].executable=="C:\\trusted\\game.exe","rule form drives validated production rule");SetDlgItemTextW(workspace,301,L"Fixture Work");SendMessageW(workspace,WM_COMMAND,310,0);Check(draft.profiles.size()==1&&draft.profiles[0].policy.monitors[selected].dim==42,"profile captures unsaved monitor policy");
    SetDlgItemTextW(workspace,343,L"unsaved.exe");ExerciseSettingsDpi(workspace,343,workspaceLayout,workspaceScroll);Check(workspaceRules.size()==1&&draft.profiles.size()==1,"workspace DPI preserves rule/profile draft");testTheme=2;RefreshSettingsTheme();Check(ControlText(workspace,343)==L"777"&&GetObjectType(workspaceFont)==OBJ_FONT,"workspace high contrast preserves draft and font");testTheme=-1;RefreshSettingsTheme();SendMessageW(workspace,WM_COMMAND,318,0);Check(!workspace&&draft.monitors[selected].dim==42&&GetDlgItemInt(editor,150,nullptr,FALSE)==42,"selected profile loads into reviewable main draft");
    ShowSaverOptions();SetDlgItemTextW(options,201,L"C:\\unsaved\\fixture.scr");auto editorHandle=editor,optionsHandle=options;topologyPending=true;SendMessageW(ui,WM_TIMER,1,0);Check(editor==editorHandle&&options==optionsHandle&&ControlText(options,201)==L"C:\\unsaved\\fixture.scr"&&draft.monitors[selected].dim==42,"topology refresh retains main/options draft by original identity");DestroyWindow(options);DestroyWindow(editor);
    ShowSetup();Check(setupWindow&&GetDlgItem(setupWindow,407),"reopenable quick setup");RECT suggested{-1200,50,-300,500};SendMessageW(setupWindow,WM_DPICHANGED,MAKEWPARAM(144,144),reinterpret_cast<LPARAM>(&suggested));Check(IsWindow(setupWindow)&&setupLayout.dpi==144,"setup DPI ownership");DestroyWindow(setupWindow);IdentifyDisplays();auto labels=identifyWindows.size();Check(labels==displays.size(),"one identity label per current output");IdentifyDisplays();Check(identifyWindows.size()==labels,"identify repeated action does not accumulate overlays");for(auto& item:identifyWindows)item.second=0;SendMessageW(ui,WM_TIMER,1,0);Check(identifyWindows.empty()&&identifyLabels.empty(),"identify timeout closes every overlay without power");
    Check(ConflictText().find(L"Windows screensaver")!=std::wstring::npos,"read-only OS conflict query reports result/errors");return 1;
}
LRESULT QuickSetupUi(){
    SetupSessions();auto savedPath=configPath;configPath=Executable()+L".quick-setup-settings";DeleteFileW(Extended(configPath).c_str());
    controller.config.monitors["disconnected"].folder="C:\\retained";
    controller.config.monitors["A"].hardware=true;controller.config.monitors["A"].powerAfter=0;
    controller.config.perInput=false;
    auto original=Serialize(controller.config);ShowSetup();Check(setupWindow&&!editor,"quick setup opens without full editor");
    Check(IsDlgButtonChecked(setupWindow,408)==BST_UNCHECKED,"existing global-input preference is visible in quick setup");
    SendDlgItemMessageW(setupWindow,408,BM_CLICK,0,0);
    Check(IsDlgButtonChecked(setupWindow,408)==BST_CHECKED&&setupEdited&&!controller.config.perInput,"independent input checkbox edits the draft only");
    for(int mode:{0,1,2}){testTheme=mode;RefreshSettingsTheme();CaptureHidden(setupWindow,mode==0?L".quick-setup-light.png":mode==1?L".quick-setup-dark.png":L".quick-setup-contrast.png");}testTheme=-1;RefreshSettingsTheme();
    LOGFONTW font{};GetObjectW(setupFont,sizeof(font),&font);Check(abs(font.lfHeight)>=MulDiv(18,setupLayout.dpi,96),"quick setup larger default type");
    SetDlgItemTextW(setupWindow,404,L"4");SendMessageW(setupWindow,WM_COMMAND,406,0);Check(setupWindow&&Serialize(controller.config)==original,"invalid idle time leaves setup and live settings intact");
    ExerciseSettingsDpi(setupWindow,404,setupLayout,setupScroll);
    testTheme=2;RefreshSettingsTheme();GetObjectW(setupFont,sizeof(font),&font);Check(abs(font.lfHeight)>=MulDiv(18,setupLayout.dpi,96)&&ControlText(setupWindow,404)==L"777","high contrast retains larger font and draft");testTheme=-1;RefreshSettingsTheme();
    CheckDlgButton(setupWindow,402,BST_UNCHECKED);SendDlgItemMessageW(setupWindow,403,CB_SETCURSEL,1,0);
    auto topology=displays;std::reverse(displays.begin(),displays.end());
    SendDlgItemMessageW(setupWindow,401,CB_SETCURSEL,1,0);SendMessageW(setupWindow,WM_COMMAND,MAKEWPARAM(401,CBN_SELCHANGE),0);
    Check(setupDraft.monitors["A"].timeout==777&&!setupDraft.monitors["A"].enabled&&setupDraft.monitors["A"].saver==8,"setup selection follows original stable display identity after reorder");
    SendMessageW(setupWindow,WM_COMMAND,4,0);Check(!setupWindow&&editor&&draft.perInput&&draft.monitors["A"].timeout==777&&Serialize(controller.config)==original,"advanced continues unsaved quick setup draft");
    ShowSetup();Check(setupWindow&&!IsWindowEnabled(editor),"quick setup owns editing while full editor draft remains open");
    SendMessageW(setupWindow,WM_COMMAND,3,0);Check(!setupWindow&&IsWindowEnabled(editor)&&draft.monitors["A"].timeout==777,"cancel restores editor without losing its draft");
    ShowSetup();SendMessageW(setupWindow,WM_COMMAND,406,0);Check(setupWindow&&editor&&controller.config.perInput&&controller.config.monitors["A"].timeout==777&&!setupEdited,"quick setup saves through canonical transaction and stays open");SendMessageW(setupWindow,WM_COMMAND,3,0);Check(!setupWindow&&IsWindowEnabled(editor),"close after save is independent and restores editor");
    Check(controller.config.monitors["A"].hardware&&controller.config.monitors["disconnected"].folder=="C:\\retained","setup preserves advanced and disconnected preferences");
    DestroyWindow(editor);ShowSetup();
    Check(IsDlgButtonChecked(setupWindow,408)==BST_CHECKED,"saved independent input persists when quick setup reopens");
    SendDlgItemMessageW(setupWindow,408,BM_CLICK,0,0);setupCloseResponse=IDYES;SendMessageW(setupWindow,WM_COMMAND,3,0);
    Check(!setupWindow&&controller.config.perInput,"discarding independent input edits preserves the saved choice");
    ShowSetup();controller.config.timeout=123;SendMessageW(setupWindow,WM_COMMAND,406,0);Check(setupWindow&&controller.config.timeout==123,"concurrent saved preference change rejects stale setup save");DestroyWindow(setupWindow);
    ShowSetup();auto current=Serialize(controller.config);{Handle held(CreateFileW(Extended(configPath).c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,0,nullptr));Check(!!held,"hold setup save target");SendMessageW(setupWindow,WM_COMMAND,406,0);Check(setupWindow&&Serialize(controller.config)==current,"failed setup save retains draft and active settings");}DestroyWindow(setupWindow);
    displays.clear();ShowSetup();Check(!IsWindowEnabled(GetDlgItem(setupWindow,402)),"no-display setup disables monitor controls");Check(StoreSetupDraft(),"no-display setup draft remains valid");DestroyWindow(setupWindow);
    displays=topology;ShowSetup();legacyStartupOverride=1;
    CheckDlgButton(setupWindow,405,BST_CHECKED);SendMessageW(setupWindow,WM_COMMAND,MAKEWPARAM(405,BN_CLICKED),0);
    SendMessageW(setupWindow,WM_COMMAND,406,0);
    Check(setupWindow&&ControlText(setupWindow,410).find(L"old OLED Aegis startup")!=std::wstring::npos&&setupEdited,"legacy blocker appears inline without discarding draft");
    CheckDlgButton(setupWindow,405,BST_UNCHECKED);SendMessageW(setupWindow,WM_COMMAND,406,0);
    Check(setupWindow&&!setupEdited&&!controller.config.automatic&&saveFailure.empty(),"manual save succeeds with legacy startup and clears old error");legacyStartupOverride=-1;
    SetDlgItemTextW(setupWindow,404,L"43");setupCloseResponse=IDNO;SendMessageW(setupWindow,WM_CLOSE,0,0);
    Check(setupWindow&&ControlText(setupWindow,404)==L"43","declining discard keeps unsaved controls");
    setupCloseResponse=IDYES;SendMessageW(setupWindow,WM_COMMAND,IDCANCEL,0);Check(!setupWindow,"Escape confirms discard separately from Save");setupCloseResponse=IDNO;
    // More than two displays: each save retains the selection and earlier saves.
    controller=Controller{};displays.clear();for(int i=0;i<6;++i){auto id="DisplayFixture"+std::to_string(i);displays.push_back({id,"",L"Panel "+std::to_wstring(i+1),nullptr,{i*100,0,(i+1)*100,100},true});}
    ShowSetup();auto window=setupWindow;
    for(int i=0;i<6;++i){
        SendDlgItemMessageW(window,401,CB_SETCURSEL,i,0);SendMessageW(window,WM_COMMAND,MAKEWPARAM(401,CBN_SELCHANGE),0);
        SetDlgItemInt(window,404,30+i,FALSE);SendDlgItemMessageW(window,403,CB_SETCURSEL,i%3,0);
        SendMessageW(window,WM_COMMAND,406,0);
        Check(setupWindow==window&&setupSelection==i&&!setupEdited&&ControlText(window,410).find(L"Setup saved")!=std::wstring::npos,"repeated Save retains window selection and success feedback");
        Config disk;std::string bytes,error;DWORD code=0;Check(ReadFileText(configPath,bytes,code)&&Parse(bytes,disk,error),"six-display save persists valid configuration");
        for(int j=0;j<=i;++j)Check(disk.monitors[displays[j].id].timeout==30+j,"each save preserves earlier display settings");
    }
    SetWindowPos(window,nullptr,0,0,620,700,SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);
    SetupStatus(L"Turn off Automatic to save now, or disable the old OLED Aegis startup entry and exit that app before enabling Automatic.");
    RECT status{},save{},close{},advanced{};GetWindowRect(GetDlgItem(window,410),&status);GetWindowRect(GetDlgItem(window,406),&save);GetWindowRect(GetDlgItem(window,3),&close);GetWindowRect(GetDlgItem(window,4),&advanced);
    Check(status.bottom<=save.top&&advanced.right<save.left&&save.right<close.left,"wrapped status and footer controls do not overlap at minimum width");
    for(int id:{4,406,3,407}){
        auto button=GetDlgItem(window,id);RECT bounds{};GetClientRect(button,&bounds);HDC dc=GetDC(button);auto buttonFont=reinterpret_cast<HFONT>(SendMessageW(button,WM_GETFONT,0,0));auto old=SelectObject(dc,buttonFont);
        RECT text{};auto label=ControlText(window,id);DrawTextW(dc,label.c_str(),-1,&text,DT_CALCRECT|DT_SINGLELINE);SelectObject(dc,old);ReleaseDC(button,dc);
        Check(text.right+MulDiv(fujin::spacing,setupLayout.dpi,96)<=bounds.right,"setup action labels fit without ellipsis");
    }
    SendMessageW(window,WM_COMMAND,3,0);Check(!setupWindow&&!setupHeadingFont,"clean close releases heading font");
    displays=topology;configPath=savedPath;SetupSessions();return 1;
}
LRESULT NextDiagnostics(){controller.config.monitors["A"].custom="C:\\Users\\PRIVATE-USER\\SECRET.scr";for(int i=0;i<300;++i)Record("PRIVATE-ID",std::string(500,'x')+"PRIVATE-SECRET",123);Record("Display 1",ReasonText(Reason::Fallback));auto report=DiagnosticText();{std::lock_guard lock(diagnosticLock);Check(diagnosticEvents.size()==128&&diagnosticEvents.front().event.size()<=96,"bounded reason/event memory");}Check(report.size()<24000&&report.find("PRIVATE")==std::string::npos&&report.find("SECRET")==std::string::npos&&report.find("C:\\")==std::string::npos&&report.find("event redacted")!=std::string::npos&&report.find("Presentation failed")!=std::string::npos,"diagnostics default redacts identities paths secrets, retains failure category");DWORD error=0;Check(WriteFileText(Executable()+L".redacted-diagnostics.txt",report,error),"diagnostics export works after prior failed operation");return 1;}
LRESULT NextMapping(){SetupSessions();ShowSettings();draft.monitors["disconnected"].enabled=false;draft.monitors["disconnected"].folder="C:\\retained";Profile compactProfile;compactProfile.name="Imported";compactProfile.policy.monitors["A"].dim=37;draft.profiles={compactProfile};ShowWorkspace();importPending=true;importSources={"source"};importTargets=displays;pendingMapping.clear();SendDlgItemMessageW(workspace,331,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(L"source"));SendDlgItemMessageW(workspace,331,CB_SETCURSEL,0,0);for(auto label:{L"Keep disconnected",L"A",L"B",L"C"})SendDlgItemMessageW(workspace,332,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(label));SendDlgItemMessageW(workspace,332,CB_SETCURSEL,1,0);auto saved=displays;displays.erase(displays.begin());SendMessageW(workspace,WM_COMMAND,319,0);Check(pendingMapping.empty(),"stale visible import target cannot silently map to another output");SendDlgItemMessageW(workspace,332,CB_SETCURSEL,2,0);SendMessageW(workspace,WM_COMMAND,319,0);Check(pendingMapping["source"]=="B","surviving import target bound by stable ID after reorder");displays=saved;SendDlgItemMessageW(workspace,300,CB_SETCURSEL,0,0);SendMessageW(workspace,WM_COMMAND,318,0);Config checked;Check(Commit(checked,draft,[](const std::string&){return true;})&&checked.monitors["A"].dim==37&&!checked.monitors["disconnected"].enabled&&checked.monitors["disconnected"].folder=="C:\\retained","profile draft load/save preserves omitted disconnected preferences");DestroyWindow(editor);return 1;}
LRESULT NextAdapterConsumer(){controller=Controller{};controller.config.controllerInput=true;controller.config.automatic=true;controller.config.media=false;controller.config.timeout=5;controller.config.monitors["A"].media=0;controller.config.monitors["B"].media=1;controller.Topology({"A","B"},4000);controller.foreground.monitors={"A"};controller.foreground.valid=true;consumedControllerInput=0;AdapterSnapshot held;held.at=held.inputAt=10000;held.active=held.connected=held.controllerAvailable=true;ConsumeAdapters(held,10000);controller.Tick(10000,{});Check(controller.nodes["A"].state==State::Desktop,"production adapter consumer credits held input");ConsumeAdapters(held,20000);controller.Tick(20000,{});Check(controller.nodes["A"].state==State::Suppressed&&controller.Explain("A",{},20000).primary==Reason::Stale&&controller.Explain("A",{},20000).kind==DeadlineKind::None,"stalled shared media observer cannot blank controller-enabled media-opt-out output");AdapterSnapshot missing;missing.at=22000;ConsumeAdapters(missing,22000);controller.Tick(22000,{});Check(!controller.activityStale&&!controller.Any(),"fresh missing API clears stale gate with fresh idle interval");controller.Tick(25000,{});Check(controller.nodes["A"].state==State::Black,"fresh inactive adapter allows ordinary idle activation");controller.config.automatic=false;controller.Reset(GetTickCount64());return 1;}
LRESULT StartDim(){controller=Controller{};controller.config.media=false;controller.config.automatic=true;controller.config.timeout=5;auto& p=controller.config.monitors["A"];p.dim=50;p.fadeMs=1000;p.dimSeconds=3;p.saver=8;p.blackAfter=5;displays={{"A","",L"Dim A",nullptr,{-640,-360,0,0},true}};auto now=GetTickCount64();controller.Topology({"A"},now-5000);controller.Tick(now,{});Reconcile();Check(runs.size()==1&&runs[0]->presentation==-3,"native dim created from production policy");auto style=GetWindowLongPtrW(runs[0]->window,GWL_EXSTYLE);Check((style&(WS_EX_LAYERED|WS_EX_TRANSPARENT|WS_EX_NOACTIVATE))==(WS_EX_LAYERED|WS_EX_TRANSPARENT|WS_EX_NOACTIVATE),"dim is layered click-through nonactivating");RECT bounds{};GetWindowRect(runs[0]->window,&bounds);Check(bounds.left==-640&&bounds.top==-360,"dim exact negative monitor bounds");BYTE alpha=0;DWORD flags=0;Check(GetLayeredWindowAttributes(runs[0]->window,nullptr,&alpha,&flags)&&alpha<=128,"dim alpha initialized before display");controller.Activity(now+1,false,"A",{},true);Reconcile();Check(runs.empty()&&powerTasks.empty(),"input immediately reverses dim with no hardware");return 1;}
LRESULT StartScenes(){controller=Controller{};controller.config.media=false;controller.config.monitors={{"A",{true,8}},{"B",{true,9}}};displays={{"A","",L"Clock",nullptr,{-640,0,0,360},true},{"B","",L"Sparse",nullptr,{0,0,640,360},true}};controller.Topology({"A","B"},GetTickCount64());ManualAll();return 1;}
LRESULT RenderScenes(bool measureResources){
    Check(runs.size()==2,"two concurrent native scenes");auto savedPadding=runs[0]->padding;
    controller.config.padding=40;controller.effectiveReady=false;controller.SelectPolicy(GetTickCount64(),0);
    Check(runs[0]->padding==savedPadding,"manual presentation retains padding across policy change");
    BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=640;info.bmiHeader.biHeight=-360;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;
    void* pixels=nullptr;HDC dc=CreateCompatibleDC(nullptr);HBITMAP bitmap=CreateDIBSection(dc,&info,DIB_RGB_COLORS,&pixels,nullptr,0);
    Check(dc&&bitmap&&pixels,"scene DIB target");auto old=SelectObject(dc,bitmap);RECT rect{0,0,640,360};size_t nonblack=0;
    // Measure steady-state growth after lazy scene fonts and GDI text resources
    // exist. Hidden windows may or may not have received WM_PAINT before this test.
    for(int frame=0;frame<4;++frame)for(auto& run:runs)PaintScene(dc,rect,*run,run->began+frame*100);
    GdiFlush();Check(runs[0]->sceneFont&&GetObjectType(runs[0]->sceneFont)==OBJ_FONT,"clock owns its reusable scene font");
    DWORD handles0=0,handles1=0;GetProcessHandleCount(GetCurrentProcess(),&handles0);auto gdi=GetGuiResources(GetCurrentProcess(),GR_GDIOBJECTS);
    FILETIME created,exited,kernel0,user0,kernel1,user1;GetProcessTimes(GetCurrentProcess(),&created,&exited,&kernel0,&user0);
    LARGE_INTEGER frequency,wall0,wall1;QueryPerformanceFrequency(&frequency);QueryPerformanceCounter(&wall0);
    for(int frame=0;frame<200;++frame)for(auto& run:runs){FillRect(dc,&rect,static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));PaintScene(dc,rect,*run,run->began+frame*100);GdiFlush();auto data=static_cast<DWORD*>(pixels);for(int i=0;i<640*360;++i)if(data[i]&0xffffff)++nonblack;}
    Check(nonblack>0&&nonblack<640*360*200,"native scenes draw sparse dim pixels");
    GetProcessTimes(GetCurrentProcess(),&created,&exited,&kernel1,&user1);auto value=[](FILETIME t){return (uint64_t(t.dwHighDateTime)<<32)|t.dwLowDateTime;};
    GetProcessHandleCount(GetCurrentProcess(),&handles1);QueryPerformanceCounter(&wall1);auto gdiAfter=GetGuiResources(GetCurrentProcess(),GR_GDIOBJECTS);
    printf("NEXTBETA %s scene render sample: 400 hidden frames after warmup; wall=%.3f ms; CPU=%.3f ms; handles=%lu->%lu; GDI=%lu->%lu; no GPU/display endurance claim\n",measureResources?"isolated":"integrated",1000.0*(wall1.QuadPart-wall0.QuadPart)/frequency.QuadPart,(value(kernel1)+value(user1)-value(kernel0)-value(user0))/10000.0,handles0,handles1,gdi,gdiAfter);
    if(measureResources)Check(handles1<=handles0+2&&gdiAfter<=gdi+2,"bounded isolated native renderer resources");
    SelectObject(dc,old);Check(DeleteObject(bitmap)&&DeleteDC(dc),"scene test target cleanup");return 1;
}
// Process-wide GDI counts must be sampled before starting UI/media workers.
// The integration pass below still exercises real Run windows and pixel output.
void IsolatedRenderScenes(){
    Check(runs.empty()&&!ui&&!uiThread,"renderer resource fixture precedes host startup");
    for(int scene:{8,9}){auto run=std::make_unique<Run>();run->presentation=scene;run->began=GetTickCount64();runs.push_back(std::move(run));}
    RenderScenes(true);GdiFlush();auto gdiBefore=GetGuiResources(GetCurrentProcess(),GR_GDIOBJECTS);runs.clear();GdiFlush();
    auto gdiAfter=GetGuiResources(GetCurrentProcess(),GR_GDIOBJECTS);
    Check(gdiBefore==gdiAfter+1,"isolated scene font released with owner");controller=Controller{};
}
LRESULT RenderScenesUi(){return RenderScenes(false);}
LRESULT StartPreviewFixture(){SetupSessions();controller.config.monitors["A"].enabled=false;controller.config.monitors["B"].enabled=false;controller.Manual("C",GetTickCount64(),true);Reconcile();ShowSettings();Preference p;p.saver=6;p.hardware=true;p.powerAfter=5;fixtureModes["@preview"]=L"--preview-responsive";launchDelay=600;Check(StartDraftPreview(p),"contained draft preview starts independently");for(auto& r:runs)if(r->contained){Check(!r->preference.hardware&&!r->preference.powerAfter&&!controller.nodes.contains("@preview"),"preview has no controller or hardware authority");SendMessageW(r->window,WM_SIZE,0,MAKELPARAM(800,500));Check(GetParent(r->preview)==r->window,"resizable child remains contained");}DestroyWindow(editor);Check(std::any_of(runs.begin(),runs.end(),[](auto& r){return r->contained&&r->cancel;}),"closing settings cancels delayed preview launch");return 1;}
LRESULT IntegrationUi(){
    SetupSessions();integrationFixture.clear();ReadIntegrationSettings();
    Check(!OpenSetupOnStart(false)&&OpenSetupOnStart(true)&&integration.trayAction==0&&integration.appearance==0,"missing native settings preserve defaults and first-run setup");
    integrationFixture={{L"StartupPresentation",L"setup"},{L"TrayClickAction",L"setup"},{L"Appearance",L"light"}};
    SendMessageW(ui,kIntegration,0,0);Check(OpenSetupOnStart(false),"startup preference loaded");
    SendMessageW(ui,kTray,0,WM_LBUTTONUP);Check(setupWindow&&!editor,"tray click opens quick setup");
    SetDlgItemTextW(setupWindow,404,L"456");auto window=setupWindow;auto baseline=Serialize(controller.config);
    integrationFixture[L"Appearance"]=L"dark";testTheme=0;SendMessageW(ui,kIntegration,0,0);
    Check(theme.dark&&setupWindow==window&&ControlText(window,404)==L"456"&&Serialize(controller.config)==baseline,"live dark override preserves unsaved setup and saved policy");
    integrationFixture[L"Appearance"]=L"light";testTheme=1;SendMessageW(ui,kIntegration,0,0);Check(!theme.dark,"light override wins over Windows dark");
    testTheme=2;SendMessageW(ui,kIntegration,0,0);Check(theme.highContrast&&theme.colors.base==GetSysColor(COLOR_WINDOW),"high contrast wins over appearance override");
    DestroyWindow(setupWindow);integrationFixture[L"TrayClickAction"]=L"settings";SendMessageW(ui,kIntegration,0,0);
    SendMessageW(ui,kTray,0,WM_LBUTTONUP);Check(editor&&!setupWindow,"tray click opens advanced editor");
    SetDlgItemTextW(editor,150,L"37");auto editorBefore=editor;integrationFixture[L"Appearance"]=L"system";testTheme=0;SendMessageW(ui,kIntegration,0,0);
    Check(!theme.dark&&editor==editorBefore&&ControlText(editor,150)==L"37","follow Windows resumes and preserves advanced edits");DestroyWindow(editor);
    integrationFixture={{L"StartupPresentation",L"invalid"},{L"TrayClickAction",L"invalid"},{L"Appearance",L"invalid"}};SendMessageW(ui,kIntegration,0,0);
    Check(!OpenSetupOnStart(false)&&integration.trayAction==0&&integration.appearance==0,"invalid native settings fall back safely");
    controller.Manual("A",GetTickCount64(),true,-1);Check(controller.Any(),"active protection fixture");SendMessageW(ui,kTray,0,WM_LBUTTONUP);Check(!controller.Any(),"default tray click stops protection");
    integrationFixture.clear();testTheme=-1;SendMessageW(ui,kIntegration,0,0);SetupSessions();return 1;
}
LRESULT StartHungPreview(){launchDelay=0;fixtureModes["@preview"]=L"--preview-hung";Preference p;p.saver=6;return StartDraftPreview(p);}
LRESULT StaleEditor(){
    SetupSessions();ShowSettings();Check(editor!=nullptr,"stale editor fixture");
    auto changed=controller.config;changed.timeout=123;Check(Save(changed,false),"external settings change");
    auto saved=Serialize(controller.config);SendMessageW(editor,WM_COMMAND,130,0);
    Check(Serialize(controller.config)==saved&&saveFailure.find(L"changed elsewhere")!=std::wstring::npos,"stale Advanced draft cannot overwrite live settings");
    ShowWorkspace();Check(workspace!=nullptr,"stale workspace fixture");SendMessageW(workspace,WM_COMMAND,312,0);
    Check(Serialize(controller.config)==saved,"stale workspace cannot overwrite live settings");
    DestroyWindow(workspace);DestroyWindow(editor);return 1;
}
LRESULT ClosePreview(){for(auto& r:runs)if(r->contained)SendMessageW(r->window,WM_CLOSE,0,0);Reconcile();return 1;}
LRESULT VerifyPreviewFallback(){for(auto& r:runs)if(r->contained){Check(r->presentation==-1&&r->done,"contained fallback consumed");ValidateRect(r->window,nullptr);for(int i=0;i<10;++i)Reconcile();Check(!GetUpdateRect(r->window,nullptr,FALSE),"contained black fallback stops repaint requests");return 1;}return 0;}
LRESULT PreviewPhotoAndExpiry(){Preference p;p.saver=7;p.folder="C:\\dac-missing-fixture-folder";Check(StartDraftPreview(p),"invalid photos preview starts safe black fallback");for(auto& r:runs)if(r->contained)r->began=GetTickCount64()-120001;Reconcile();return 1;}
LRESULT StartBatterySaver(){SetupSessions();controller.config.monitors["A"].batteryBlack=true;controller.Manual("A",GetTickCount64(),true);Reconcile();return 1;}
LRESULT BatteryBlack(){auto& node=controller.nodes["A"];auto began=node.began,oldGeneration=node.generation;auto window=runs[0]->window;controller.powerSource=PowerSource::DC;controller.Tick(GetTickCount64(),{});Reconcile();Check(node.state==State::Black&&node.began==began&&runs[0]->window==window&&runs[0]->presentation==-1&&!IsWindowVisible(runs[0]->preview),"battery replaces expensive content behind same opaque cover without resetting origin");Check(!controller.Result("A",oldGeneration,true)&&powerTasks.empty(),"late result cannot revive retired content or request power");ValidateRect(window,nullptr);for(int i=0;i<10;++i)Reconcile();Check(!GetUpdateRect(window,nullptr,FALSE),"retired battery black stops repaint requests");controller.powerSource=PowerSource::AC;controller.Tick(GetTickCount64(),{});Check(node.state==State::Black&&controller.config.monitors["A"].saver==1,"AC return preserves current black and saved assignment");return 1;}
LRESULT StartRevocation(){controller.Reset(GetTickCount64());controller.config.monitors["A"].hardware=true;controller.effectiveReady=false;controller.Manual("A",GetTickCount64(),true,-1);fakePowerResult=0;fakePowerWakeResult=0;fakePowerDelay=0;fakePowerOff=fakePowerOn=0;Check(StartPower(displays[0],controller.nodes["A"].generation)==1,"fake off starts under explicit grant");return 1;}
LRESULT HardwareDiagnostics(){Reconcile();auto text=DiagnosticText();Check(text.find("hardware off accepted")!=std::string::npos&&text.find("hardware wake accepted")!=std::string::npos,"actual fake off/revocation wake outcomes exported");for(int status:{2,3}){auto task=std::make_unique<PowerTask>();task->id="PRIVATE-TICKET-TARGET";task->status=status;task->done=true;powerTasks.push_back(std::move(task));Reconcile();}text=DiagnosticText();Check(text.find("hardware unavailable")!=std::string::npos&&text.find("hardware wake failed")!=std::string::npos&&text.find("PRIVATE-TICKET-TARGET")==std::string::npos,"injected hardware failure outcomes exported without recovery identity");return 1;}
void NextBeta(){IsolatedRenderScenes();ControllerAdapterTests();CloseHandle(stopEvent);stopEvent=nullptr;Check(Initialize(),"next beta initializes");OnUi(SetupSessions);OnUi(NextMigration);OnUi(NextAdapterConsumer);OnUi(SetupSessions);OnUi(NextUi);OnUi(QuickSetupUi);OnUi(IntegrationUi);OnUi(StaleEditor);OnUi(NextMapping);OnUi(NextDiagnostics);OnUi(StopSessions);Until([]{return snapshot.runs==0;},3000,"next UI cleanup");OnUi(StartDim);OnUi(StartScenes);Until([]{return snapshot.runs==2&&snapshot.a==State::Saver&&snapshot.b==State::Saver;},2000,"two independent native scenes");OnUi(RenderScenesUi);OnUi(StopSessions);Until([]{return snapshot.runs==0;},2000,"scene timers/windows/fonts stop");
    OnUi(StartPreviewFixture);Until([]{return snapshot.runs==1&&snapshot.jobs==0;},3000,"preview closed during launch cleans only preview");Check(snapshot.c==State::Black,"unrelated production black survives preview close");OnUi(StartHungPreview);Until([]{return snapshot.runs==2&&snapshot.jobs==1;},2000,"hung preview initially launches contained job");Until([]{return snapshot.runs==2&&snapshot.jobs==0;},6500,"hung preview job health cleanup");OnUi(VerifyPreviewFallback);OnUi(ClosePreview);Until([]{return snapshot.runs==1;},2000,"hung preview close");OnUi(PreviewPhotoAndExpiry);Until([]{return snapshot.runs==1;},2500,"invalid photo/preview maximum duration cleanup");OnUi(StopSessions);Until([]{return snapshot.runs==0;},2000,"pre-battery cleanup");
    OnUi(StartBatterySaver);Until([]{return snapshot.a==State::Saver;},2500,"battery fixture saver ready");OnUi(BatteryBlack);Until([]{return snapshot.jobs==0;},3000,"battery retires child behind black");OnUi(StopSessions);Until([]{return snapshot.runs==0;},2000,"battery stop");OnUi(StartRevocation);Until([]{return snapshot.powerPhase==1;},2000,"fake off accepted and reported on UI owner");Check(fakePowerOff==1,"fake off observed");OnUi(+[]()->LRESULT{SendMessageW(ui,WM_TIMER,1,0);Check(!controller.hardwareFaults.contains("A"),"healthy owned power ticket is not fault quarantine");controller.config.monitors["A"].hardware=false;Reconcile();return 1;});Until([]{return snapshot.powers==0;},3000,"terminal power outcome observed before task drain");Check(fakePowerOn==1,"grant revocation requests fake recovery wake");OnUi(HardwareDiagnostics);
    OnUi(StopSessions);Until([]{return snapshot.runs==0;},3000,"next beta final sessions stop");Shutdown();injectedObservations=false;Check(runs.empty()&&powerTasks.empty()&&ownedWindows.empty()&&ownedJobs.empty()&&!workspace&&!setupWindow,"next beta all owners released");puts("NEXTBETA PASS: production policy/native UI, schema2 migration and byte-exact rollback, recovery retention, rules/profiles/mapping, hidden DPI/topology, dim/scenes/preview, XInput injection, battery and fake permission-revocation wake, bounded redacted diagnostics. No real off/lock/OS-setting mutation.");}

// These fixtures call the shipping dispatch/controller/native hide and joined
// maintenance paths, with hidden HWNDs, synthetic events and fake tray/DDC only.
int wakeScreens=3,wakeStyle=-1,wakeTarget=0;DWORD wakeAge=0;
MSG wakeMessage{};HANDLE wakeBlockEntered{},wakeProcessed{};DWORD wakeBlockMs=0;
Time measuredQueue=0,measuredHandler=0;std::map<std::string,Node> wakeBefore;
LRESULT NativeSetup(){
    Reset();Reconcile();Check(runs.empty(),"native setup drains previous sessions");
    injectedObservations=true;controller=Controller{};controller.config.media=false;controller.config.perInput=true;controller.config.automatic=false;
    displays.clear();std::vector<std::string> ids;auto now=GetTickCount64();
    for(int i=0;i<wakeScreens;++i){auto id=std::to_string(i);ids.push_back(id);Display d{};d.id=id;d.identified=true;d.rect={-800+i*800,-200,i*800,400};displays.push_back(d);controller.config.monitors[id].saver=wakeStyle;}
    controller.SelectPolicy(now-20000,0);controller.Topology(ids,now-20000);
    for(auto& id:ids)controller.Manual(id,now-7000,false,wakeStyle);
    Reconcile();Check(runs.size()==ids.size(),"native black/clock/constellation sessions created");
    for(auto& run:runs)Check(!run->worker.joinable()&&!IsWindowVisible(run->window),"native fixtures hidden without saver workers");
    wakeBefore=controller.nodes;InvalidateFaults();return 1;
}
void NativeDeliver(MSG message,Time now){RAWINPUT input{};input.header.dwType=RIM_TYPEMOUSE;input.data.mouse.lLastX=1;auto start=Micros();DeliverInput(input,message,now);measuredHandler=Micros()-start;measuredQueue=static_cast<DWORD>(now)-message.time;}
void VerifyNativeWake(){
    auto target=std::to_string(wakeTarget);Check(!Running(controller.nodes.at(target).state),"one queued movement wakes intended native display without a second packet");
    for(auto& [id,before]:wakeBefore)if(id!=target){auto after=controller.nodes.at(id);Check(after.generation==before.generation&&after.last==before.last&&after.state==before.state,"untouched display preserves generation state and idle history");}
    for(auto& run:runs)if(run->id==target)Check(run->cancel&&!IsWindowVisible(run->window),"wake-only reconcile hides target immediately");
    Reconcile();Check(runs.size()==static_cast<size_t>(wakeScreens-1),"native target alone retired");
}
MSG NativeMessage(Time now,DWORD age){MSG message{};message.message=WM_INPUT;message.time=static_cast<DWORD>(now-age);message.pt={-400+wakeTarget*800,100};return message;}
LRESULT NativeSample(){NativeSetup();auto now=GetTickCount64();NativeDeliver(NativeMessage(now,wakeAge),now);VerifyNativeWake();return 1;}
LRESULT NativeBlock(){SetEvent(wakeBlockEntered);Sleep(wakeBlockMs);return 1;}
LRESULT NativeQueued(){NativeDeliver(wakeMessage,GetTickCount64());VerifyNativeWake();SetEvent(wakeProcessed);return 1;}
LRESULT NativeSemantics(){
    Time at=0;DWORD age=0;Check(MessageTimeAt(0x100000020ULL,0xfffffff0u,at,age)&&at==0xfffffff0ULL&&age==48,"DWORD message timestamp wrap");
    Check(!MessageTimeAt(1000,1001,at,age)&&!MessageTimeAt(1000,0x800003e8u,at,age),"future and half-range stamps rejected");
    NativeSetup();auto now=GetTickCount64();RAWINPUT input{};input.header.dwType=RIM_TYPEMOUSE;auto message=NativeMessage(now,300);
    auto topologySerial=controller.serial;controller.Topology({"replacement"},now-100);displays[0].id="replacement";controller.Manual("replacement",now-50,false,-1);DeliverInput(input,message,now);
    Check(Running(controller.nodes.at("replacement").state)&&controller.serial>topologySerial,"pre-topology queued pointer cannot wake replacement output");
    NativeSetup();now=GetTickCount64();message=NativeMessage(now,300);controller.Reset(now-100);controller.Manual("0",now-50,false,-1);DeliverInput(input,message,now);Check(Running(controller.nodes.at("0").state),"pre-reset event cannot wake new manual session");
    NativeSetup();now=GetTickCount64();message=NativeMessage(now,300);controller.Manual("0",now-100,false,-1);DeliverInput(input,message,now);Check(Running(controller.nodes.at("0").state),"queued event before manual activation rejected");
    NativeSetup();now=GetTickCount64();message=NativeMessage(now,300);controller.nodes["0"].manual=false;controller.nodes["0"].began=now-100;DeliverInput(input,message,now);Check(Running(controller.nodes.at("0").state),"queued event before automatic activation rejected");
    NativeSetup();now=GetTickCount64();message=NativeMessage(now,300);topologyPending=true;DeliverInput(input,message,now);Check(controller.Any(),"pending topology rejects old coordinates");topologyPending=false;
    NativeSetup();now=GetTickCount64();auto original=controller.nodes["1"];controller.config.monitors["1"].enabled=false;controller.Activity(now,true,"0",{"1"},true,now-300,false);Check(controller.nodes["1"].generation==original.generation,"disabled and delayed keyboard focus do not get credited");
    for(int mode:{1,2,3}){NativeSetup();now=GetTickCount64();controller.config.monitors["1"].input=mode;controller.manualPreferences["1"].input=mode;
        controller.Activity(now,true,"",{"1"},true,now-100,true);Check(Running(controller.nodes["1"].state)==(mode==2),"fresh keyboard known focus remains usable without pointer attribution");}
    for(int mode:{1,2,3}){NativeSetup();now=GetTickCount64();controller.manualPreferences["1"].input=mode;controller.Activity(now,true,"0",{"1"},true,now-300,false);Check(Running(controller.nodes["1"].state),"delayed keyboard does not invent historical focus");}
    NativeSetup();now=GetTickCount64();controller.manualPreferences["1"].input=0;controller.Activity(now,false,"0",{},true,now-300,false);Check(!Running(controller.nodes["0"].state)&&!Running(controller.nodes["1"].state)&&Running(controller.nodes["2"].state),"explicit shared override only wakes opted-in peer");
    NativeSetup();now=GetTickCount64();controller.nodes["0"].sticky=true;NativeDeliver(NativeMessage(now,300),now);Check(Running(controller.nodes["0"].state),"sticky ignores valid queued activity");
    NativeSetup();now=GetTickCount64();auto generation=controller.nodes["0"].generation;controller.config.timeout=61;controller.effectiveReady=false;controller.SelectPolicy(now,0);controller.Activity(now,false,"0",{},true,now-300,false);Check(!Running(controller.nodes["0"].state)&&controller.nodes["0"].generation!=generation,"policy refresh preserving manual session preserves queued wake");
    NativeSetup();now=GetTickCount64();controller.nodes["@span"]={now-7000,++controller.serial,State::Black,true,now-7000};controller.Activity(now,false,"",{},false,now-300,false);Check(!Running(controller.nodes["@span"].state),"spanning accepts valid unknown activity");
    for(USHORT flags:{USHORT(RI_MOUSE_LEFT_BUTTON_DOWN),USHORT(RI_MOUSE_LEFT_BUTTON_UP),USHORT(RI_MOUSE_WHEEL),USHORT(0)}){NativeSetup();now=GetTickCount64();input.data.mouse.usButtonFlags=flags;input.data.mouse.lLastX=flags?0:1;DeliverInput(input,NativeMessage(now,300),now);VerifyNativeWake();}
    // Crossing: the first event's point is A while the next queued event belongs
    // to B. No current-cursor query is available to rewrite either observation.
    NativeSetup();now=GetTickCount64();wakeTarget=0;NativeDeliver(NativeMessage(now,1500),now);Check(Running(controller.nodes["1"].state),"queued crossing first targets negative-coordinate A");wakeTarget=1;NativeDeliver(NativeMessage(now,300),now);Check(!Running(controller.nodes["1"].state)&&Running(controller.nodes["2"].state),"queued crossing second targets B only");wakeTarget=0;
    NativeSetup();now=GetTickCount64();message=NativeMessage(now,300);message.pt={50000,-50000};displays[0].rect={49000,-51000,51000,-49000};NativeDeliver(message,now);VerifyNativeWake();
    NativeSetup();now=GetTickCount64();message=NativeMessage(now,300);message.pt={90000,90000};NativeDeliver(message,now);for(auto& [id,before]:wakeBefore)Check(controller.nodes[id].generation==before.generation,"unknown pointer never wakes every independent output");
    Reset();Reconcile();return 1;
}
void NativeRaw(bool fallback=false){
    wake_fixture::enabled=true;wake_fixture::packet={};wake_fixture::packet.header.dwType=RIM_TYPEMOUSE;wake_fixture::packet.data.mouse.lLastX=1;
    wake_fixture::message=NativeMessage(GetTickCount64(),300);wake_fixture::message.lParam=1;
    dispatchedMessage=fallback?MSG{}:wake_fixture::message;injectedObservations=false;Input(reinterpret_cast<HRAWINPUT>(1));injectedObservations=true;dispatchedMessage={};wake_fixture::enabled=false;
}
LRESULT NativeRawTests(){
    NativeSetup();NativeRaw();VerifyNativeWake();NativeSetup();NativeRaw(true);VerifyNativeWake();
    NativeSetup();auto now=GetTickCount64();wake_fixture::enabled=true;wake_fixture::readFailure=true;wake_fixture::message=NativeMessage(now,300);wake_fixture::message.lParam=1;dispatchedMessage=wake_fixture::message;controller.manualPreferences["1"].input=0;injectedObservations=false;Input(reinterpret_cast<HRAWINPUT>(1));injectedObservations=true;
    Check(Running(controller.nodes["0"].state)&&!Running(controller.nodes["1"].state)&&Running(controller.nodes["2"].state),"raw read failure preserves only explicit shared wake");
    controller.Manual("1",now-100,false,-1);injectedObservations=false;Input(reinterpret_cast<HRAWINPUT>(1));injectedObservations=true;Check(Running(controller.nodes["1"].state),"raw failure cannot bypass manual activation timestamp");
    wake_fixture::enabled=false;wake_fixture::readFailure=false;dispatchedMessage={};
    NativeSetup();now=GetTickCount64();auto original=controller.nodes["1"];controller.config.monitors["1"].enabled=false;controller.Activity(now,false,"1",{},true,now-100,true);Check(controller.nodes["1"].generation==original.generation&&controller.nodes["1"].last==original.last,"fresh targeted input respects disabled output");
    NativeSetup();now=GetTickCount64();wake_fixture::enabled=true;wake_fixture::message=NativeMessage(now,300);wake_fixture::message.time=static_cast<DWORD>(controller.inputBoundary);wake_fixture::packet.header.dwType=RIM_TYPEMOUSE;injectedObservations=false;Input(reinterpret_cast<HRAWINPUT>(1));injectedObservations=true;wake_fixture::enabled=false;Check(controller.nodes["0"].generation==wakeBefore["0"].generation,"same-ms topology boundary is ambiguous and rejected");
    for(DWORD delay:{0u,300u}){NativeSetup();wake_fixture::focus=CreateWindowExW(WS_EX_TOOLWINDOW,kClass,L"",WS_POPUP,0,-200,800,600,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);Check(wake_fixture::focus,"hidden synthetic focus window");wake_fixture::enabled=true;wake_fixture::packet.header.dwType=RIM_TYPEKEYBOARD;wake_fixture::message=NativeMessage(GetTickCount64(),delay);dispatchedMessage={};injectedObservations=false;Input(reinterpret_cast<HRAWINPUT>(1));injectedObservations=true;wake_fixture::enabled=false;
        Check(!Running(controller.nodes["0"].state)&&Running(controller.nodes["1"].state)==(delay>250)&&Running(controller.nodes["2"].state),"raw keyboard distinguishes fresh focus from aged focus");DestroyWindow(wake_fixture::focus);wake_fixture::focus=nullptr;}
    NativeSetup();wake_fixture::focus=CreateWindowExW(WS_EX_TOOLWINDOW,kClass,L"",WS_POPUP,0,-200,800,600,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);Check(wake_fixture::focus,"focus without pointer fixture");wake_fixture::enabled=true;wake_fixture::packet.header.dwType=RIM_TYPEKEYBOARD;wake_fixture::message=NativeMessage(GetTickCount64(),0);wake_fixture::message.pt={90000,90000};dispatchedMessage={};injectedObservations=false;Input(reinterpret_cast<HRAWINPUT>(1));injectedObservations=true;wake_fixture::enabled=false;
    Check(controller.nodes["0"].generation==wakeBefore["0"].generation&&!Running(controller.nodes["1"].state)&&controller.nodes["2"].generation==wakeBefore["2"].generation,"actual fresh raw keyboard credits known focus with no pointer attribution");DestroyWindow(wake_fixture::focus);wake_fixture::focus=nullptr;
    Reset();Reconcile();return 1;
}
LRESULT PreparePostedRaw(){NativeSetup();wake_fixture::packet={};wake_fixture::packet.header.dwType=RIM_TYPEMOUSE;wake_fixture::packet.data.mouse.lLastX=1;wake_fixture::message=NativeMessage(GetTickCount64(),300);wake_fixture::message.pt={50000,-50000};displays[0].rect={49000,-51000,51000,-49000};wake_fixture::enabled=true;wake_fixture::overrideQueue=true;injectedObservations=false;return 1;}
LRESULT VerifyPostedRaw(){injectedObservations=true;wake_fixture::enabled=false;wake_fixture::overrideQueue=false;VerifyNativeWake();return 1;}
void WaitMaintenance(bool tray=false){Time deadline=GetTickCount64()+4000;while(GetTickCount64()<deadline){bool busy=tray?maintenanceInTray.load():maintenanceInFile.load();if(busy)return;Sleep(1);}Check(false,"maintenance worker entered injected stall");}
LRESULT NativeMaintain(){Maintain(GetTickCount64());return 1;}
LRESULT NativeResourceInput(){
    NativeSetup();auto now=GetTickCount64();auto message=NativeMessage(now,0);DWORD before=0,after=0;GetProcessHandleCount(GetCurrentProcess(),&before);auto gdi=GetGuiResources(GetCurrentProcess(),GR_GDIOBJECTS);auto count=diagnosticEvents.size();
    for(int i=0;i<50000;++i)NativeDeliver(message,now);
    GetProcessHandleCount(GetCurrentProcess(),&after);Check(after<=before&&GetGuiResources(GetCurrentProcess(),GR_GDIOBJECTS)<=gdi&&diagnosticEvents.size()==count,"50000 packets retain no handles GDI or diagnostic packets");
    {std::lock_guard lock(maintenanceLock);Check(maintenancePending.paths.size()<=32&&maintenancePending.resets.size()<=32,"maintenance pending slots bounded");}
    auto report=DiagnosticText();Check(report.size()<24000&&report.find("sessionTopologyBoundary=")!=std::string::npos&&report.find("Wake timing maxima")!=std::string::npos&&report.find("C:\\")==std::string::npos,"bounded redacted aggregate diagnostics");Reset();Reconcile();return 1;
}
// Shared unchanged-source/candidate workload. Timing includes the shipping raw
// wrapper, attribution, policy and hiding; fixtures never inject physical input.
uint64_t PerfNanos(){return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());}
void PerfSummary(const char* stage,int style,int coverage,std::vector<uint64_t>& samples){
    std::sort(samples.begin(),samples.end());uint64_t total=0;for(auto value:samples)total+=value;
    printf("WAKEPERF %s style=%d coverage=%d samples=%zu mean=%llu p50=%llu p95=%llu p99=%llu max=%llu ns\n",stage,style,coverage,samples.size(),static_cast<unsigned long long>(total/samples.size()),static_cast<unsigned long long>(samples[samples.size()/2]),static_cast<unsigned long long>(samples[samples.size()*95/100]),static_cast<unsigned long long>(samples[samples.size()*99/100]),static_cast<unsigned long long>(samples.back()));
}
LRESULT WakePerformanceUi(){
    KillTimer(ui,1);injectedObservations=true;locked=suspended=displayOff=false;
    for(int style:{-1,8,9})for(int coverage:{0,1,2,3}){
        wakeScreens=3;wakeTarget=0;wakeStyle=style;NativeSetup();Reconcile();
        std::vector<uint64_t> samples;samples.reserve(2000);
        wake_fixture::enabled=true;wake_fixture::packet={};wake_fixture::packet.header.dwType=RIM_TYPEMOUSE;wake_fixture::packet.data.mouse.lLastX=1;
        for(int sample=-100;sample<2000;++sample){
            auto now=GetTickCount64();wake_fixture::message=NativeMessage(now,0);wake_fixture::message.lParam=1;dispatchedMessage=wake_fixture::message;
            // Rearm outside the measured interval, with existing hidden shells.
            // Every timed active sample actually accepts an active target.
            for(auto& [id,node]:controller.nodes){node.state=std::stoi(id)<coverage?(style<0?State::Black:State::Saver):State::Desktop;node.manual=false;node.last=now-10;node.began=now-10;}
            for(auto& run:runs){run->cancel=false;run->generation=controller.nodes.at(run->id).generation;}
            injectedObservations=false;auto began=PerfNanos();Input(reinterpret_cast<HRAWINPUT>(1));auto elapsed=PerfNanos()-began;injectedObservations=true;if(sample>=0)samples.push_back(elapsed);
        }
        wake_fixture::enabled=false;dispatchedMessage={};PerfSummary("raw",style,coverage,samples);
        Reset();Reconcile();
    }
    for(int style:{-1,8,9})for(int coverage:{1,2,3}){
        wakeScreens=coverage;wakeStyle=style;NativeSetup();Reconcile();std::vector<uint64_t> paint,timer;paint.reserve(200);timer.reserve(200);
        for(int sample=-10;sample<200;++sample){
            auto began=PerfNanos();for(auto& run:runs){InvalidateRect(run->window,nullptr,FALSE);WindowProc(run->window,WM_PAINT,0,0);}auto elapsed=PerfNanos()-began;if(sample>=0)paint.push_back(elapsed);
            began=PerfNanos();WindowProc(ui,WM_TIMER,1,0);elapsed=PerfNanos()-began;if(sample>=0)timer.push_back(elapsed);
        }
        PerfSummary("paint",style,coverage,paint);PerfSummary("timer",style,coverage,timer);Reset();Reconcile();
    }
    printf("WAKEPERF counterBytes=%zu",sizeof(wakeTiming));
#ifdef DAC_WAKE_OBSERVABILITY
    printf(" policyCounterBytes=%zu",sizeof(controller.inputDiagnostics));
#endif
    puts("; hidden/synthetic timing, no input-to-photon measurement");return 1;
}
void WakePerformance(){
    DWORD before=0,after=0;GetProcessHandleCount(GetCurrentProcess(),&before);auto gdiBefore=GetGuiResources(GetCurrentProcess(),GR_GDIOBJECTS);
    CloseHandle(stopEvent);stopEvent=nullptr;Check(Initialize(),"performance fixture initialize");OnUi(WakePerformanceUi);Shutdown();
    GetProcessHandleCount(GetCurrentProcess(),&after);auto gdiAfter=GetGuiResources(GetCurrentProcess(),GR_GDIOBJECTS);
    Check(!ui&&runs.empty()&&ownedWindows.empty()&&ownedJobs.empty()&&!maintenanceWorker.joinable(),"performance harness ownership drained");
    printf("WAKEPERF cleanup handles=%lu->%lu GDI=%lu->%lu ownership=empty\n",before,after,gdiBefore,gdiAfter);
}

#ifdef DAC_WAKE_OBSERVABILITY
Time BinTotal(const TimingBins& bins){Time total=0;for(auto& bin:bins.bins)SaturatingAdd(total,bin.load());return total;}
int coverageCount=0;unsigned coverageMask=0;bool coverageAutomatic=false;
LRESULT NativeCoverageSetup(){
    Reset();Reconcile();Check(runs.empty(),"coverage setup drains existing sessions");injectedObservations=true;
    controller=Controller{};controller.config.media=false;controller.config.perInput=true;controller.config.automatic=coverageAutomatic;controller.config.timeout=5;
    displays.clear();std::vector<std::string> ids;auto now=GetTickCount64();coverageCount=0;
    for(int i=0;i<wakeScreens;++i){auto id=std::to_string(i);ids.push_back(id);Display d{};d.id=id;d.identified=true;d.rect={-800+i*800,-200,i*800,400};displays.push_back(d);controller.config.monitors[id].saver=wakeStyle;}
    controller.SelectPolicy(now-20000,0);controller.Topology(ids,now-20000);
    for(int i=0;i<wakeScreens;++i){bool active=!!(coverageMask&(1u<<i));if(active)++coverageCount;
        if(coverageAutomatic)controller.nodes[ids[i]].last=now-(active?20000:7000);else if(active)controller.Manual(ids[i],now-7000,false,wakeStyle);
    }
    if(coverageAutomatic)controller.Tick(now-7000,{});Reconcile();Reconcile();
    Check(runs.size()==static_cast<size_t>(coverageCount),"initial coverage creates exactly the protected owners");
    for(auto& [id,node]:controller.nodes){bool active=!!(coverageMask&(1u<<std::stoi(id)));Check(node.state==(active?(wakeStyle<0?State::Black:State::Saver):State::Desktop),"initial native coverage uses steady Black or Saver, not Launching");}
    for(auto& run:runs)Check(!run->worker.joinable()&&!IsWindowVisible(run->window),"steady coverage is hidden and has no saver worker");wakeBefore=controller.nodes;return 1;
}
LRESULT NativeCoverageSample(){
    NativeCoverageSetup();std::map<std::string,std::tuple<Time,Time,int,HWND>> owners;
    for(auto& run:runs)owners[run->id]={run->generation,run->began,run->presentation,run->window};
    auto cancelled=wakeTiming.newlyCancelled.load(),hides=wakeTiming.hideRequests.load(),hidden=wakeTiming.postHidden.load();
    auto samples=BinTotal(wakeTiming.dispatchUs);auto empty=wakeTiming.coverageEmpty.load(),partial=wakeTiming.coveragePartial.load(),full=wakeTiming.coverageFull.load();
    auto now=GetTickCount64();NativeDeliver(NativeMessage(now,wakeAge),now);auto target=std::to_string(wakeTarget);bool active=!!(coverageMask&(1u<<wakeTarget));
    Check(controller.nodes.at(target).state==State::Desktop,"coverage target actually reaches desktop");
    Check(controller.inputDiagnostics.activeAccepted==Time(active)&&controller.inputDiagnostics.activeToDesktop==Time(active)&&controller.inputDiagnostics.desktopAccepted==Time(!active),"active versus desktop acceptance is counted exactly");
    Check(wakeTiming.newlyCancelled==cancelled+active&&wakeTiming.hideRequests==hides+active&&wakeTiming.postHidden==hidden+active,"new cancellation, hide request and observed hidden state counted separately");
    Check(BinTotal(wakeTiming.dispatchUs)==samples+1,"one dispatch histogram sample per delivered packet");
    Check(wakeTiming.coverageEmpty==empty+(coverageCount==0)&&wakeTiming.coverageFull==full+(coverageCount==wakeScreens)&&wakeTiming.coveragePartial==partial+(coverageCount>0&&coverageCount<wakeScreens),"empty/partial/full initial coverage samples are distinct");
    ReconcileWake();Check(wakeTiming.newlyCancelled==cancelled+active,"repeated reconcile never counts an owner as newly cancelled twice");
    for(auto& [id,before]:wakeBefore)if(id!=target){auto& after=controller.nodes.at(id);Check(after.generation==before.generation&&after.last==before.last&&after.began==before.began&&after.state==before.state&&after.manual==before.manual&&after.sticky==before.sticky&&after.stageBegan==before.stageBegan&&after.presentation==before.presentation&&after.powerRequested==before.powerRequested,"untouched complete node state retained for partial/full coverage");}
    Reconcile();Check(runs.size()==static_cast<size_t>(coverageCount-int(active)),"only an active target owner is retired");
    for(auto& run:runs){auto before=owners.at(run->id);Check(std::make_tuple(run->generation,run->began,run->presentation,run->window)==before&&!run->cancel,"untouched owner lifetime identity and presentation retained");}
    return 1;
}
LRESULT NativeStageAccounting(){
    wakeScreens=3;wakeStyle=9;coverageMask=7;coverageAutomatic=false;NativeCoverageSetup();
    auto receipts=wakeTiming.receipts.load(),calls=wakeTiming.rawCalls.load(),reads=wakeTiming.readSuccess.load(),direct=wakeTiming.directMetadata.load(),nested=wakeTiming.nestedMetadata.load();
    auto rawSamples=BinTotal(wakeTiming.rawUs);NativeRaw();
    Check(wakeTiming.rawCalls==calls+1&&wakeTiming.readSuccess==reads+1&&wakeTiming.directMetadata==direct+1&&wakeTiming.nestedMetadata==nested&&wakeTiming.receipts==receipts&&BinTotal(wakeTiming.rawUs)==rawSamples+1,"wrapper reads/direct metadata differ from actual WM_INPUT receipt");
    NativeCoverageSetup();NativeRaw(true);Check(wakeTiming.nestedMetadata==nested+1,"nested message metadata counted separately");
    NativeCoverageSetup();wake_fixture::enabled=true;wake_fixture::packet={};wake_fixture::packet.header.dwType=RIM_TYPEMOUSE;wake_fixture::message=NativeMessage(GetTickCount64(),300);wake_fixture::message.lParam=1;dispatchedMessage=wake_fixture::message;injectedObservations=false;
    auto fixtureReads=wake_fixture::rawReads.load();WindowProc(ui,WM_INPUT,RIM_INPUTSINK,1);injectedObservations=true;wake_fixture::enabled=false;dispatchedMessage={};
    Check(wakeTiming.receipts==receipts+1&&wake_fixture::rawReads==fixtureReads+1,"actual WM_INPUT receipt and raw adapter read separately observed");
    NativeCoverageSetup();auto unsupported=wakeTiming.unsupported.load();RAWINPUT input{};input.header.dwType=RIM_TYPEHID;auto now=GetTickCount64();auto valid=wakeTiming.validTime.load();DeliverInput(input,NativeMessage(now,0),now);
    Check(wakeTiming.unsupported==unsupported+1&&wakeTiming.validTime==valid&&!controller.inputDiagnostics.activeToDesktop,"unsupported raw type never reaches timestamp or policy stages");
    input.header.dwType=RIM_TYPEMOUSE;auto message=NativeMessage(now,0);message.time=static_cast<DWORD>(now+1);auto invalid=wakeTiming.invalidTime.load();DeliverInput(input,message,now);
    Check(wakeTiming.invalidTime==invalid+1&&!controller.inputDiagnostics.activeToDesktop,"invalid timestamp has no accepted transition");
    NativeCoverageSetup();now=GetTickCount64();auto unique=wakeTiming.pointerUnique.load(),absent=wakeTiming.pointerAbsent.load(),ambiguous=wakeTiming.pointerAmbiguous.load();
    displays[1].rect=displays[0].rect;DeliverInput(input,NativeMessage(now,0),now);Check(wakeTiming.pointerAmbiguous==ambiguous+1&&!controller.inputDiagnostics.activeToDesktop,"overlapping display rectangles are ambiguous without wake-all");
    message=NativeMessage(now,0);message.pt={90000,90000};DeliverInput(input,message,now);Check(wakeTiming.pointerAbsent==absent+1&&wakeTiming.pointerUnique==unique&&!controller.inputDiagnostics.activeToDesktop,"absent pointer attribution preserves all presentations");
    NativeCoverageSetup();now=GetTickCount64();auto unavailable=wakeTiming.foregroundUnavailable.load(),fresh=wakeTiming.foregroundFresh.load();
    wake_fixture::enabled=true;wake_fixture::focus=CreateWindowExW(WS_EX_TOOLWINDOW,kClass,L"PRIVATE-WAKE-TITLE",WS_POPUP,0,-200,800,600,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);Check(wake_fixture::focus,"synthetic focus fixture");
    input.header.dwType=RIM_TYPEKEYBOARD;auto queries=wake_fixture::foregroundQueries.load();DeliverInput(input,NativeMessage(now,300),now);
    Check(wake_fixture::foregroundQueries==queries&&wakeTiming.foregroundUnavailable==unavailable+1&&wakeTiming.foregroundFresh==fresh,"delayed input does not query or invent historical focus");
    NativeCoverageSetup();now=GetTickCount64();DeliverInput(input,NativeMessage(now,0),now);Check(wake_fixture::foregroundQueries==queries+1&&wakeTiming.foregroundFresh==fresh+1,"fresh synthetic foreground attribution counted");
    DestroyWindow(wake_fixture::focus);wake_fixture::focus=nullptr;wake_fixture::enabled=false;
    NativeCoverageSetup();auto failed=wakeTiming.readFailures.load(),rawFailed=wakeTiming.rawFailure.load();valid=wakeTiming.validTime.load();reads=wakeTiming.readSuccess.load();
    wake_fixture::enabled=true;wake_fixture::readFailure=true;wake_fixture::message=NativeMessage(GetTickCount64(),300);wake_fixture::message.lParam=1;dispatchedMessage=wake_fixture::message;controller.manualPreferences["1"].input=0;injectedObservations=false;Input(reinterpret_cast<HRAWINPUT>(1));injectedObservations=true;dispatchedMessage={};wake_fixture::readFailure=false;wake_fixture::enabled=false;
    Check(wakeTiming.readFailures==failed+1&&wakeTiming.rawFailure==rawFailed+1&&wakeTiming.readSuccess==reads&&wakeTiming.validTime==valid+1&&controller.inputDiagnostics.activeToDesktop==1&&Running(controller.nodes["0"].state)&&!Running(controller.nodes["1"].state),"failed raw read retains shared-only semantics with actual fallback transition accounting");
    for(bool automatic:{false,true}){coverageAutomatic=automatic;NativeCoverageSetup();now=GetTickCount64();input.header.dwType=RIM_TYPEMOUSE;message=NativeMessage(now,8000);auto rejected=wakeTiming.boundaryRejected.load();DeliverInput(input,message,now);Check(controller.inputDiagnostics.activeToDesktop==0&&wakeTiming.boundaryRejected==rejected+1,"steady manual/automatic activation rejects older input");}
    coverageAutomatic=false;Reset();Reconcile();return 1;
}
LRESULT NativeResetAccounting(){
    wakeScreens=3;wakeStyle=8;coverageMask=7;coverageAutomatic=false;
    for(auto cause:{ResetCause::Session,ResetCause::Power,ResetCause::Tray,ResetCause::Topology}){
        locked=suspended=displayOff=topologyPending=false;trayPresent=true;NativeCoverageSetup();auto now=GetTickCount64();auto message=NativeMessage(now,300);auto before=wakeTiming.resets[static_cast<size_t>(cause)].load();
        switch(cause){case ResetCause::Session:WindowProc(ui,WM_WTSSESSION_CHANGE,WTS_SESSION_LOCK,0);break;case ResetCause::Power:WindowProc(ui,WM_POWERBROADCAST,PBT_APMSUSPEND,0);break;case ResetCause::Tray:WindowProc(ui,taskbar,0,0);break;case ResetCause::Topology:WindowProc(ui,WM_DISPLAYCHANGE,0,0);break;default:Check(false,"reset fixture cause");}
        Check(wakeTiming.resets[static_cast<size_t>(cause)]==before+1&&!controller.Any(),"actual session/power/tray/topology message resets are separately counted");
        auto nodes=controller.nodes;RAWINPUT input{};input.header.dwType=RIM_TYPEMOUSE;auto boundary=wakeTiming.timestampBoundary.load(),pending=wakeTiming.topologyRejected.load();DeliverInput(input,message,GetTickCount64());
        Check(wakeTiming.timestampBoundary==boundary+1&&wakeTiming.topologyRejected==pending+topologyPending,"reset timestamp and pending topology rejection distinguished");
        for(auto& [id,node]:nodes)Check(controller.nodes[id].generation==node.generation&&controller.nodes[id].last==node.last,"old packet cannot change reset generation or idle history");Reconcile();Check(runs.empty(),"reset native owners retire");
    }
    locked=suspended=displayOff=topologyPending=false;trayPresent=true;controller.blocked=false;
    std::atomic<Time> counter{UINT64_MAX-2};Count(counter,9);Count(counter);Check(counter==UINT64_MAX,"atomic counter saturates rather than wrapping");
    TimingBins bins;for(Time value:{0ULL,1ULL,2ULL,4ULL,5ULL,16ULL,17ULL,64ULL,65ULL,256ULL,257ULL,1024ULL,1025ULL,4096ULL,4097ULL,UINT64_MAX})Sample(bins,value);
    for(auto& bin:bins.bins)Check(bin==2,"timing histogram edge and overflow bins exact");bins.bins.back()=UINT64_MAX;Sample(bins,UINT64_MAX);Check(bins.bins.back()==UINT64_MAX,"histogram overflow bin saturates");
    std::atomic<Time> concurrent{UINT64_MAX-100};std::thread one([&]{for(int i=0;i<1000;++i)Count(concurrent);}),two([&]{for(int i=0;i<1000;++i)Count(concurrent);});one.join();two.join();Check(concurrent==UINT64_MAX,"concurrent aggregate saturation exact");
    auto heartbeat=wakeTiming.heartbeatSamples.load(),timer=BinTotal(wakeTiming.timerUs),paint=wakeTiming.paintSamples.load();heartbeatAt=GetTickCount64()-300;WindowProc(ui,WM_TIMER,1,0);
    Check(wakeTiming.heartbeatSamples==heartbeat+1&&BinTotal(wakeTiming.timerUs)==timer+1&&wakeTiming.heartbeatMaxMs>=300,"actual timer and heartbeat stages counted");WindowProc(ui,WM_PAINT,0,0);Check(wakeTiming.paintSamples==paint+1,"actual paint stage counted");
    auto text=DiagnosticText();Check(text.find("activeToDesktop=")!=std::string::npos&&text.find("pointerAmbiguous=")!=std::string::npos&&text.find("compositor disappearance unmeasured")!=std::string::npos&&text.find("PRIVATE")==std::string::npos&&text.find("raw us bins")!=std::string::npos,"new aggregate diagnostics expose stages and preserve redaction");
    Reset();Reconcile();return 1;
}
void NativeCoverageMatrix(){
    int samples=0;for(int style:{-1,8,9})for(int count:{1,2,3})for(unsigned mask=0;mask<(1u<<count);++mask)for(bool automatic:{false,true})for(int target=0;target<count;++target)for(DWORD delay:{0u,300u,1500u,5000u}){
        wakeStyle=style;wakeScreens=count;coverageMask=mask;coverageAutomatic=automatic;wakeTarget=target;wakeAge=delay;OnUi(NativeCoverageSample);++samples;
    }
    wakeTarget=0;OnUi(NativeStageAccounting);OnUi(NativeResetAccounting);
    printf("NATIVEWAKE coverage: %d hidden steady samples; every empty/partial/full subset on 1/2/3 displays, black/clock/constellation, manual/automatic, desktop/active target and 0/300/1500/5000-ms input; visible desktop unexecuted\n",samples);
}

#endif // DAC_WAKE_OBSERVABILITY: unchanged-source performance comparison

void NativeWake(){
    CloseHandle(stopEvent);stopEvent=nullptr;Check(Initialize(),"native wake initialize");OnUi(+[]()->LRESULT{KillTimer(ui,1);injectedObservations=true;locked=suspended=displayOff=false;return 1;});
    Time maximum=0;int samples=0;
    for(int style:{-1,8,9})for(int count:{1,2,3})for(int target=0;target<count;++target)for(DWORD delay:{0u,300u,1500u,5000u}){wakeStyle=style;wakeScreens=count;wakeTarget=target;wakeAge=delay;OnUi(NativeSample);maximum=std::max(maximum,measuredHandler);++samples;}
    printf("NATIVEWAKE matrix: %d hidden samples black/clock/constellation, 1/2/3 displays, every target, 0/300/1500/5000 ms queue age; handler max=%llu us; visible rendering unmeasured\n",samples,maximum);
    wakeScreens=3;wakeTarget=0;wakeStyle=-1;OnUi(NativeSemantics);OnUi(NativeRawTests);OnUi(PreparePostedRaw);PostMessageW(ui,WM_INPUT,RIM_INPUTSINK,1);
    // Sent messages can overtake posted messages; wait for the UI callback posted after WM_INPUT.
    wakeProcessed=CreateEventW(nullptr,TRUE,FALSE,nullptr);PostMessageW(ui,WM_APP+101,0,reinterpret_cast<LPARAM>(+[]()->LRESULT{VerifyPostedRaw();SetEvent(wakeProcessed);return 1;}));Check(WaitForSingleObject(wakeProcessed,2000)==WAIT_OBJECT_0,"actual UI message loop captures full queued MSG point");CloseHandle(wakeProcessed);
    wakeBlockEntered=CreateEventW(nullptr,TRUE,FALSE,nullptr);wakeProcessed=CreateEventW(nullptr,TRUE,FALSE,nullptr);
    for(int style:{-1,8,9})for(DWORD delay:{300u,1500u,5000u}){wakeStyle=style;wakeBlockMs=delay;OnUi(NativeSetup);ResetEvent(wakeBlockEntered);ResetEvent(wakeProcessed);PostMessageW(ui,WM_APP+101,0,reinterpret_cast<LPARAM>(NativeBlock));Check(WaitForSingleObject(wakeBlockEntered,1000)==WAIT_OBJECT_0,"UI stall entered");wakeMessage=NativeMessage(GetTickCount64(),0);PostMessageW(ui,WM_APP+101,0,reinterpret_cast<LPARAM>(NativeQueued));Check(WaitForSingleObject(wakeProcessed,delay+2000)==WAIT_OBJECT_0,"one posted movement wakes after UI resumes");printf("NATIVEWAKE blocked UI style=%d injected=%lu queue=%llu ms resume-handler=%llu us; no second movement\n",style,delay,measuredQueue,measuredHandler);}
    CloseHandle(wakeBlockEntered);CloseHandle(wakeProcessed);
    maintenanceFileDelay=1500;OnUi(+[]()->LRESULT{NativeSetup();InvalidateFaults();Maintain(GetTickCount64());return 1;});WaitMaintenance();
    OnUi(+[]()->LRESULT{auto before=Micros();WindowProc(ui,WM_TIMER,1,0);Check(Micros()-before<1000000,"actual timer remains responsive during 1500-ms file stall");return 1;});OnUi(NativeSample);Check(measuredHandler<1000000,"slow maintenance does not serialize unrelated input");printf("NATIVEWAKE 1500-ms background file stall: independent handler=%llu us\n",measuredHandler);
    // Invalidate/reset during an old scan; reservation blocks power until actual
    // deletion completes, even though its stale result cannot alter the cache.
    OnUi(+[]()->LRESULT{DWORD error=0;Check(WriteFileText(PowerMarker("0"),"fixture",error),"fault fixture");RequestFaultReset("0");Check(FaultResetPending("0"),"reset reserves power target");controller.config.monitors["0"].hardware=true;Check(StartPower(displays[0],controller.nodes["0"].generation)==-1,"reset defers new power ticket without consuming attempt");InvalidateFaults();Maintain(GetTickCount64());return 1;});maintenanceFileDelay=0;
    Time deadline=GetTickCount64()+4000;while(GetTickCount64()<deadline){OnUi(NativeMaintain);bool pending=false;OnUi(+[]()->LRESULT{return 1;});{std::lock_guard lock(maintenanceLock);pending=maintenancePending.faultEpoch||!maintenancePending.resets.empty()||maintenanceResultReady;}if(!maintenanceInFile&&!pending)break;Sleep(10);}
    OnUi(+[]()->LRESULT{Maintain(GetTickCount64());Check(!FaultResetPending("0")&&!PowerPending("0"),"fault reset completes and deletes fixture");Check(wakeTiming.staleResults>0,"stale fault completion rejected");return 1;});
    auto failures=wakeTiming.fileFailures.load();maintenanceFileFail=true;OnUi(+[]()->LRESULT{InvalidateFaults();Maintain(GetTickCount64());return 1;});
    deadline=GetTickCount64()+4000;while(wakeTiming.fileFailures==failures&&GetTickCount64()<deadline){OnUi(NativeMaintain);Sleep(5);}
    OnUi(+[]()->LRESULT{Maintain(GetTickCount64());Check(CachedFault("0")&&wakeTiming.fileFailures>0,"failed refresh cannot clear quarantine");return 1;});maintenanceFileFail=false;
    maintenanceTrayDelay=1200;trayFailures=1;OnUi(+[]()->LRESULT{trayPresent=false;requestedTray=-1;nextTray=0;Maintain(GetTickCount64());return 1;});WaitMaintenance(true);
    for(int i=0;i<15;++i){OnUi(NativeMaintain);Sleep(100);}maintenanceTrayDelay=0;trayFailures=0;
    deadline=GetTickCount64()+4000;while(GetTickCount64()<deadline){OnUi(NativeMaintain);bool readyTray=false;DWORD_PTR value=0;SendMessageTimeoutW(ui,WM_APP+101,0,reinterpret_cast<LPARAM>(+[]()->LRESULT{return trayPresent?2:1;}),SMTO_ABORTIFHUNG,2000,&value);readyTray=value==2;if(readyTray)break;Sleep(20);}
    OnUi(+[]()->LRESULT{Check(trayPresent,"slow/failing tray converges through controlled retry");auto calls=wakeTiming.trayCalls.load();for(int i=0;i<100;++i)Maintain(GetTickCount64());Check(wakeTiming.trayCalls==calls,"unchanged tray does not repeatedly call shell");return 1;});
    // A successful add can finish after Explorer loss/new desired state. Reject
    // that response and converge from the worker's actual shell state.
    maintenanceTrayDelay=250;auto staleBefore=wakeTiming.staleResults.load();
    OnUi(+[]()->LRESULT{Tray();return 1;});WaitMaintenance(true);
    OnUi(+[]()->LRESULT{WindowProc(ui,taskbar,0,0);Check(!trayPresent&&controller.blocked,"Explorer loss blocks immediately");Maintain(GetTickCount64());return 1;});
    Sleep(300);maintenanceTrayDelay=0;OnUi(+[]()->LRESULT{Maintain(GetTickCount64());Check(!trayPresent,"stale successful tray result cannot unblock");return 1;});
    deadline=GetTickCount64()+4000;while(GetTickCount64()<deadline){OnUi(NativeMaintain);DWORD_PTR value=0;SendMessageTimeoutW(ui,WM_APP+101,0,reinterpret_cast<LPARAM>(+[]()->LRESULT{return trayPresent?2:1;}),SMTO_ABORTIFHUNG,2000,&value);if(value==2)break;Sleep(10);}
    OnUi(+[]()->LRESULT{Check(trayPresent&&!controller.blocked,"latest tray request recovers after stale success");return 1;});Check(wakeTiming.staleResults>staleBefore,"stale successful tray completion counted");
    #ifdef DAC_WAKE_OBSERVABILITY
    NativeCoverageMatrix();
    #endif
    OnUi(NativeResourceInput);maintenanceFileDelay=250;OnUi(+[]()->LRESULT{InvalidateFaults();Maintain(GetTickCount64());return 1;});WaitMaintenance();auto shutdown=GetTickCount64();Shutdown();maintenanceFileDelay=0;
    Check(!maintenanceWorker.joinable()&&!ui&&runs.empty()&&resettingFaults.empty(),"pending maintenance joined before windows and DLL resources released");
    printf("NATIVEWAKE shutdown joined pending worker in %llu ms; queueMax=%llu ms fileMax=%llu us trayMax=%llu us; fixed counters only\n",GetTickCount64()-shutdown,wakeTiming.queueMaxMs.load(),wakeTiming.fileMaxUs.load(),wakeTiming.trayMaxUs.load());
    puts("NATIVEWAKE PASS: queued attribution, crossings, signed coordinates, timestamp wrap, generation barriers, keyboard scopes, sticky/span/disabled, clicks/wheel/held motion, slow/failing/stale maintenance, reset reservation, tray recovery, bounded input and joined shutdown. Hidden tests; no physical timing qualification.");
}

void IdlePerformance() {
    CloseHandle(stopEvent);stopEvent=nullptr;
    auto start=GetTickCount64();Check(Initialize(),"isolated idle initializes");auto started=GetTickCount64();
    Sleep(1000);FILETIME created{},exited{},kernel0{},user0{},kernel1{},user1{};
    DWORD handles0=0,handles1=0;PROCESS_MEMORY_COUNTERS_EX memory{};
    auto ticks=[](FILETIME f){return (uint64_t(f.dwHighDateTime)<<32)|f.dwLowDateTime;};
    Check(GetProcessTimes(GetCurrentProcess(),&created,&exited,&kernel0,&user0),"idle CPU baseline");
    GetProcessHandleCount(GetCurrentProcess(),&handles0);auto gdi0=GetGuiResources(GetCurrentProcess(),GR_GDIOBJECTS);auto measured=GetTickCount64();
    Sleep(10000);auto elapsed=GetTickCount64()-measured;
    Check(GetProcessTimes(GetCurrentProcess(),&created,&exited,&kernel1,&user1),"idle CPU result");
    GetProcessHandleCount(GetCurrentProcess(),&handles1);auto gdi1=GetGuiResources(GetCurrentProcess(),GR_GDIOBJECTS);
    GetProcessMemoryInfo(GetCurrentProcess(),reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&memory),sizeof(memory));
    double cpuMs=double(ticks(kernel1)+ticks(user1)-ticks(kernel0)-ticks(user0))/10000.0;
    printf("IDLEPERF interval_ms=%llu CPU_ms=%.3f one_core_percent=%.4f working_set_bytes=%zu private_bytes=%zu handles=%lu->%lu GDI=%lu->%lu startup_ms=%llu\n",elapsed,cpuMs,100.0*cpuMs/elapsed,static_cast<size_t>(memory.WorkingSetSize),static_cast<size_t>(memory.PrivateUsage),handles0,handles1,gdi0,gdi1,started-start);
    std::vector<uint64_t> latency;
    for(int i=0;i<200;++i){auto began=Micros();DWORD_PTR answer=0;Check(SendMessageTimeoutW(ui,WM_NULL,0,0,SMTO_ABORTIFHUNG,1000,&answer),"idle UI responds");latency.push_back(Micros()-began);}
    std::sort(latency.begin(),latency.end());printf("IDLEPERF UI_roundtrip_us p50=%llu p95=%llu max=%llu (private desktop, no shell tray qualification)\n",latency[100],latency[190],latency.back());
    auto stopping=GetTickCount64();Shutdown();printf("IDLEPERF shutdown_ms=%llu\n",GetTickCount64()-stopping);
    Check(handles1<=handles0+4&&gdi1<=gdi0,"idle bounded handle and GDI resources");
}
void StandaloneHostPreferences() {
    DWORD error=0;loginStartupFixture=0;
    Check(LoginStartupState()==0,"startup default absent");
    Check(SetLoginStartup(true,error)&&LoginStartupState()==1,"explicit startup enable through fake registry");
    Check(SetLoginStartup(false,error)&&LoginStartupState()==0,"startup disable through fake registry");
    loginStartupFixture=2;Check(!SetLoginStartup(true,error)&&loginStartupFixture==2,"foreign startup value preserved on enable");
    Check(!SetLoginStartup(false,error)&&loginStartupFixture==2,"foreign startup value preserved on disable");
    loginStartupFixture=-1;Check(!SetLoginStartup(true,error),"startup query failure is closed");loginStartupFixture=0;
    auto command=LoginCommand();Check(command.starts_with(L"\"")&&command.ends_with(L"\" --background"),"startup command quotes executable");
    auto localExe=Executable();auto folder=localExe.substr(0,localExe.find_last_of(L'\\'));
    Check(LocalPath(localExe,false)&&LocalPath(folder,true),"existing local fixture paths accepted");
    Check(!LocalFile(L"\\\\server\\share\\untrusted.scr",L".scr")&&!LocalFile(localExe+L":payload.scr",L".scr")&&!LocalFile(L"relative.scr",L".scr"),"UNC, alternate streams, relative saver paths rejected");
    Check(!SetConfigDirectory(folder.substr(0,3))&&!SetConfigDirectory(localExe)&&!SetConfigDirectory(L"\\\\server\\share"),"config root/file/network rejection");
    Check(SetConfigDirectory(folder+L"\\")&&configDirectoryOverride==folder,"explicit config directory normalized");
    Check(ConfigPath()==folder+L"\\settings-v1.ini"&&LoginCommand().find(L"--config-directory \""+folder+L"\"")!=std::wstring::npos,"login retains isolated profile path");
    configDirectoryOverride.clear();
    auto saverFixture=localExe+L".fixture.scr";Check(CopyFileW(localExe.c_str(),saverFixture.c_str(),FALSE),"create local saver path fixture without executing it");
    Check(LocalFile(saverFixture,L".scr"),"explicit existing local saver path accepted");DeleteFileW(saverFixture.c_str());
    Check(!objectScope.empty()&&objectScope.starts_with(L"Local\\DAC-S-1-"),"singleton names include current user SID and session");
    IntegrationSettings next{true,2,2};Check(WriteIntegrationSettings(next,error),"host setting update");ReadIntegrationSettings();
    Check(integration.startupSetup&&integration.trayAction==2&&integration.appearance==2,"native preference read applies saved settings");
    next.appearance=42;Check(!WriteIntegrationSettings(next,error)&&integration.appearance==2,"invalid host preference does not change saved setting");
    configPath=Executable()+L".host-persistence";std::string text;auto serialized=SerializeIntegration(integration);
    Check(WriteFileText(configPath,serialized,error)&&ReadFileText(configPath,text,error)&&text==serialized,"host serialized preferences atomic file roundtrip");
    integrationFixture.clear();DeleteFileW(Extended(configPath).c_str());
    puts("STANDALONE HOST PASS: preferences schema/storage, fake startup opt-in/conflict, quoted path, SID/session identity; live Run key untouched");
}
int wmain(int argc,wchar_t** argv) {
    if(argc==5&&wcscmp(argv[1],L"--power-helper")==0) {
        if(wcscmp(argv[4],L"2")==0)return 0; // models missing helper completion, not a successful hardware cycle
        configPath=Executable()+L".power-tests";std::string id,token=Utf8(argv[3]);Check(Unhex(Utf8(argv[2]),id),"helper identity");
        Handle cancelled(OpenEventW(SYNCHRONIZE,FALSE,PowerCancelName(token).c_str()));Check(cancelled,"helper cancellation event");
        return PowerCycle(id,token,[&]{return WaitForSingleObject(cancelled,0)!=WAIT_TIMEOUT;},[&](bool wake){DWORD error=0;return wake?0:(WriteFileText(PowerMarker(id),id+"\n"+token+"\noff",error)?0:11);});
    }
    if(argc>2&&(wcscmp(argv[1],L"--preview-responsive")==0||wcscmp(argv[1],L"--preview-hung")==0)) {
        WNDCLASSW wc{};wc.lpfnWndProc=DefWindowProcW;wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=L"Dac-Beta-Owned-Fixture";
        Check(RegisterClassW(&wc),"fixture class");HWND parent=reinterpret_cast<HWND>(static_cast<uintptr_t>(_wcstoui64(argv[2],nullptr,10)));
        HWND child=CreateWindowExW(0,wc.lpszClassName,L"",WS_CHILD,0,0,100,100,parent,nullptr,wc.hInstance,nullptr);Check(child,"owned fixture child");
        Time start=GetTickCount64();bool hang=wcscmp(argv[1],L"--preview-hung")==0;
        while(IsWindow(child)) {Pump();if(hang&&GetTickCount64()-start>750)Sleep(INFINITE);Sleep(5);}return 0;
    }
    if(argc>1&&wcscmp(argv[1],L"--hang")==0) {Sleep(INFINITE);return 0;}
    if(argc>1&&wcscmp(argv[1],L"--early")==0) return 23;
    if(argc>1&&wcscmp(argv[1],L"--tree")==0) {
        auto exe=Executable();auto command=L"\""+exe+L"\" --hang";
        STARTUPINFOW si{};si.cb=sizeof(si);PROCESS_INFORMATION pi{};
        Check(CreateProcessW(exe.c_str(),command.data(),nullptr,nullptr,FALSE,0,nullptr,nullptr,&si,&pi),"grandchild creation");
        CloseHandle(pi.hThread);CloseHandle(pi.hProcess);Sleep(INFINITE);return 0;
    }
    stopEvent=CreateEventW(nullptr,TRUE,FALSE,nullptr);Check(stopEvent,"stop event");
    if(argc>1&&wcscmp(argv[1],L"--host-preferences")==0){StandaloneHostPreferences();return 0;}
    if(argc>1&&wcscmp(argv[1],L"--idleperf")==0){IdlePerformance();return 0;}
    if(argc>1&&wcscmp(argv[1],L"--wakeperf")==0){WakePerformance();return 0;}
    if(argc>1&&wcscmp(argv[1],L"--nativewake")==0){NativeWake();return 0;}
    if(argc>1&&wcscmp(argv[1],L"--nextbeta")==0){NextBeta();return 0;}
    if(argc>1&&wcscmp(argv[1],L"--advanced")==0){Advanced();return 0;}
    if(argc>1&&wcscmp(argv[1],L"--power")==0){PowerTests();CloseHandle(stopEvent);return 0;}
    if(argc>1&&wcscmp(argv[1],L"--slides")==0){Slides();return 0;}
    if(argc>1&&(wcscmp(argv[1],L"--sessions")==0||wcscmp(argv[1],L"--session-soak")==0)) {Sessions(wcscmp(argv[1],L"--session-soak")==0);return 0;}
    if(argc>1&&wcscmp(argv[1],L"--faults")==0) {
        WorkerFixture(L"--early");WorkerFixture(L"--hang");
        Run missing;missing.path=Executable()+L".missing";RunWorker(&missing);Check(missing.done&&missing.status==2&&missing.error==ERROR_FILE_NOT_FOUND,"missing file fallback");
    } else if(argc>1&&(wcscmp(argv[1],L"--containment")==0||wcscmp(argv[1],L"--host-death")==0)) {
        Run run;run.path=Executable();run.args=L"--tree";run.configuration=true;std::thread worker(RunWorker,&run);
        std::vector<HANDLE> children;Time began=GetTickCount64();
        while(children.size()!=2&&GetTickCount64()-began<3000) {
            Sleep(50);std::lock_guard lock(registryLock);
            for(auto& [job,window]:ownedJobs) {
                (void)window;ULONG_PTR ids[20]{};auto list=reinterpret_cast<JOBOBJECT_BASIC_PROCESS_ID_LIST*>(ids);
                if(QueryInformationJobObject(job,JobObjectBasicProcessIdList,list,sizeof(ids),nullptr)&&list->NumberOfProcessIdsInList==2)
                    for(DWORD i=0;i<2;++i) { children.push_back(OpenProcess(SYNCHRONIZE|PROCESS_QUERY_LIMITED_INFORMATION,FALSE,static_cast<DWORD>(list->ProcessIdList[i])));wprintf(L"CHILD %lu\n",static_cast<DWORD>(list->ProcessIdList[i])); }
            }
        }
        fflush(stdout);Check(children.size()==2,"child and grandchild contained before shutdown");
        if(wcscmp(argv[1],L"--host-death")==0) {
            if(argc!=3){fputs("Host-death test requires its runner-owned receipt path\n",stderr);return 2;}
            // The runner owns this pre-created IPC receipt. Avoid rename while
            // it polls the file; configuration atomic replacement is tested separately.
            auto ids=std::to_string(GetProcessId(children[0]))+"\n"+std::to_string(GetProcessId(children[1]))+"\n";
            Handle receipt(CreateFileW(argv[2],GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr));DWORD written=0;
            Check(receipt&&WriteFile(receipt,ids.data(),static_cast<DWORD>(ids.size()),&written,nullptr)&&written==ids.size()&&FlushFileBuffers(receipt),"publish exact contained descendant identities");
            CloseHandle(receipt.value);receipt.value=nullptr;
            puts("HOST-DEATH READY");fflush(stdout);Sleep(INFINITE);
        }
        SetEvent(stopEvent);worker.join();for(auto h:children) {Check(h&&WaitForSingleObject(h,2000)==WAIT_OBJECT_0,"descendant exits");CloseHandle(h);}
        Check(ownedJobs.empty(),"job registry empty");puts("PLATFORM containment PASS");
    } else if(argc>1&&wcscmp(argv[1],L"--emergency")==0) {
        safetyReady=CreateEventW(nullptr,TRUE,FALSE,nullptr);safetyThread=CreateThread(nullptr,0,SafetyMain,nullptr,0,nullptr);
        Check(WaitForSingleObject(safetyReady,2000)==WAIT_OBJECT_0&&safetyOK,"emergency hotkey registered");
        launchDelay=1500;Run run;run.path=Executable();run.args=L"--hang";std::thread worker(RunWorker,&run);
        Time wait=GetTickCount64();bool published=false;
        while(!published&&GetTickCount64()-wait<1000) {{std::lock_guard lock(registryLock);published=!ownedJobs.empty();}Sleep(5);}
        Check(published,"stalled launch published job");Time began=GetTickCount64();SetEvent(stopEvent);
        Check(WaitForSingleObject(safetyThread,500)==WAIT_OBJECT_0,"emergency not blocked by stalled launch");
        printf("PLATFORM emergency watcher finished during stalled launch: %llu ms\n",GetTickCount64()-began);
        worker.join();Check(run.done&&ownedJobs.empty(),"late launch cancelled before child execution");
        CloseHandle(safetyThread);safetyThread=nullptr;CloseHandle(safetyReady);safetyReady=nullptr;
    } else if(argc>1&&wcscmp(argv[1],L"--lifecycle")==0) {
        CloseHandle(stopEvent);stopEvent=nullptr;
        for(int fault=1;fault<=3;++fault) {initFault=fault;Check(!Initialize(),"partial init failure");Shutdown();Check(!ui&&!singleton&&runs.empty()&&ownedJobs.empty(),"partial init unwind");}
        initFault=0;DWORD baseline=0,final=0,guiStart=0,guiEnd=0;PROCESS_MEMORY_COUNTERS_EX memoryStart{},memoryEnd{};
        Time began=GetTickCount64();
        for(int i=0;i<52;++i) {
            Check(Initialize(),"same-source host init (tray-exempt harness)");
            DWORD_PTR result=0;Check(SendMessageTimeoutW(ui,WM_APP+100,0,0,SMTO_ABORTIFHUNG,2000,&result)&&result,"hidden settings create/destroy font cleanup");
            Sleep(10);Shutdown();Shutdown();
            Check(!ui&&!editor&&!singleton&&runs.empty()&&ownedJobs.empty()&&ownedWindows.empty(),"repeated cleanup empty ownership");
            if(i==1) {GetProcessHandleCount(GetCurrentProcess(),&baseline);guiStart=GetGuiResources(GetCurrentProcess(),GR_GDIOBJECTS);GetProcessMemoryInfo(GetCurrentProcess(),reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&memoryStart),sizeof(memoryStart));}
        }
        GetProcessHandleCount(GetCurrentProcess(),&final);guiEnd=GetGuiResources(GetCurrentProcess(),GR_GDIOBJECTS);
        printf("HARNESS lifecycle 50 cycles after 2 warmups: %llu ms; handles %lu -> %lu; GDI %lu -> %lu; tray exempt; isolated native harness\n",GetTickCount64()-began,baseline,final,guiStart,guiEnd);
        GetProcessMemoryInfo(GetCurrentProcess(),reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&memoryEnd),sizeof(memoryEnd));
        printf("HARNESS private bytes %zu -> %zu; working set %zu -> %zu\n",static_cast<size_t>(memoryStart.PrivateUsage),static_cast<size_t>(memoryEnd.PrivateUsage),static_cast<size_t>(memoryStart.WorkingSetSize),static_cast<size_t>(memoryEnd.WorkingSetSize));
        Check(final<=baseline+4&&guiEnd<=guiStart,"no sustained handle/GDI growth");return 0;
    } else if(argc>1&&wcscmp(argv[1],L"--probes")==0) {
        WNDCLASSW wc{};wc.lpfnWndProc=DefWindowProcW;wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=L"Dac-Production-Harness-Preview";
        Check(RegisterClassW(&wc),"probe class");
        for(int i=0;i<6;++i) {
            if(!Available(i)) continue;
            wprintf(L"BEGIN stock structural probe %ls\n",savers[i]);fflush(stdout);
            Run run;run.window=CreateWindowExW(WS_EX_NOACTIVATE,wc.lpszClassName,L"",WS_POPUP,0,0,640,360,nullptr,nullptr,wc.hInstance,nullptr);
            Check(run.window,"hidden preview parent");run.preview=run.window;run.path=SaverPath(i);run.args=L"/p "+std::to_wstring(reinterpret_cast<uintptr_t>(run.window));
            std::thread worker(RunWorker,&run);Time began=GetTickCount64();
            while(!run.done&&run.status==0&&GetTickCount64()-began<6000) {Pump();Sleep(10);}
            int observed=run.status;run.cancel=true;
            while(!run.done&&GetTickCount64()-began<9000) {Pump();Sleep(10);}
            worker.join();DestroyWindow(run.window);
            Check(run.done&&ownedJobs.empty(),"stock worker cleanup");
            wprintf(L"RESULT %ls heuristic=%d cleanup=pass elapsed=%llu (hidden window; no visible rendering claim)\n",savers[i],observed,GetTickCount64()-began);fflush(stdout);
        }
        UnregisterClassW(wc.lpszClassName,wc.hInstance);
    } else if(argc>1&&wcscmp(argv[1],L"--media")==0) {
        HRESULT hr=CoInitializeEx(nullptr,COINIT_MULTITHREADED);Check(SUCCEEDED(hr),"media COM");
        Config c;auto m=ObserveMedia(c,Catalog());printf("PLATFORM read-only media sample valid=%d any=%d ambiguous=%d (no playback attribution qualification)\n",m.valid,m.any,m.ambiguous);
        Check(ImageName(GetCurrentProcessId()).find(L"platform-tests.exe")!=std::wstring::npos,"limited rights process image query");
        Check(ImageName(0xffffffff).empty(),"exited/inaccessible process query defined empty");CoUninitialize();
    } else if(argc>1&&wcscmp(argv[1],L"--storage")==0) {
        auto base=Executable();base=base.substr(0,base.find_last_of(L'\\'))+L"\\storage-\u03bb";
        Check(CreateDirectoryW(Extended(base).c_str(),nullptr)||GetLastError()==ERROR_ALREADY_EXISTS,"test directory");
        for(int i=0;i<4;++i) {base+=L"\\"+std::wstring(70,L'x');Check(CreateDirectoryW(Extended(base).c_str(),nullptr)||GetLastError()==ERROR_ALREADY_EXISTS,"long directory");}
        auto path=base+L"\\settings.ini";DWORD error=0;std::string data;
        Config c;c.monitors["unicode-\xce\xbb"]={false,2};
        auto victim=base+L"\\unrelated.txt",legacyTemp=path+L".tmp-"+std::to_wstring(GetCurrentProcessId());
        Check(WriteFileText(victim,"must survive",error),"temporary link target fixture");
        DeleteFileW(Extended(legacyTemp).c_str());
        Check(CreateHardLinkW(Extended(legacyTemp).c_str(),Extended(victim).c_str(),nullptr),"pre-existing predictable temporary hard link");
        Check(WriteFileText(path,Serialize(c),error)&&ReadFileText(path,data,error)&&data==Serialize(c),"Unicode long path roundtrip");
        Check(ReadFileText(victim,data,error)&&data=="must survive","save must not truncate temporary link target");
        Check(GetFileAttributesW(Extended(legacyTemp).c_str())!=INVALID_FILE_ATTRIBUTES,"save must not remove foreign temporary path");
        DeleteFileW(Extended(legacyTemp).c_str());DeleteFileW(Extended(victim).c_str());
        Handle held(CreateFileW(Extended(path).c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,0,nullptr));
        auto before=Serialize(c);Check(!Commit(c,Config{},[&](const std::string& s){return WriteFileText(path,s,error);})&&Serialize(c)==before,"denied replacement retains active config");
        Check(ReadFileText(path,data,error)&&data==before,"denied replacement retains saved bytes");
        Check(!WriteFileText(L"relative.ini","x",error),"relative path rejected");
        Check(!WriteFileText(base+L"\\missing\\file.ini","x",error),"missing directory write fails");
        Check(!ReadFileText(L"C:\\missing-dac-file.ini",data,error),"missing read fails");
        puts("PLATFORM storage PASS: Unicode, >260 chars, replacement denied, original intact, invalid/missing paths");
    } else if(argc>1&&wcscmp(argv[1],L"--catalog")==0) {
        BOOL wow64=FALSE;Check(IsWow64Process(GetCurrentProcess(),&wow64),"query test host architecture");
        for(int i=0;i<6;++i) if(Available(i)) {
            DWORD binary=0;Check(GetBinaryTypeW(SaverPath(i).c_str(),&binary),"installed saver PE type");
            if(wow64) Check(binary==SCS_64BIT_BINARY&&SaverPath(i).find(L"\\Sysnative\\")!=std::wstring::npos,"32-bit host selects native 64-bit savers");
        }
        printf("PLATFORM host pointer bits=%zu WOW64=%d; native saver selection verified\n",sizeof(void*)*8,wow64);
        auto list=Catalog();for(auto& d:list) wprintf(L"DISPLAY identified=%d rect=%ld,%ld,%ld,%ld\n",d.identified,d.rect.left,d.rect.top,d.rect.right,d.rect.bottom);
        for(int i=0;i<6;++i) wprintf(L"SAVER %ls available=%d\n",savers[i],Available(i));
        puts("PLATFORM catalog only; no rendering/desktop qualification");
    } else {fprintf(stderr,"Use --faults, --containment, --host-death, --storage, --catalog\n");return 2;}
    CloseHandle(stopEvent);stopEvent=nullptr;return 0;
}
