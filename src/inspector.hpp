// SPDX-License-Identifier: MIT
#pragma once
// UI-owner cached state only. No filesystem, media, shell or hardware calls.
constexpr wchar_t kInspector[]=L"DAC-DisplayInspector";
enum InspectorControl {InspectorList=901,InspectorDetail,InspectorStatus,InspectorRescan,InspectorExport,InspectorSettings,InspectorMap,InspectorControls};
HWND inspector{};HFONT inspectorFont{};SettingsLayout inspectorLayout;
bool inspectorRegistered=false;int inspectorScroll=0;
#ifdef DAC_HARNESS
BOOL inspectorFontDeleted=TRUE;
#endif
std::string inspectorSelected;std::vector<std::string> inspectorIds;std::vector<std::wstring> inspectorRows;
bool SnapshotStale(Time now){return DisplaySnapshotIsStale(now);}
std::wstring InspectorText(const Display& d,Time now) {
    auto p=controller.Pref(d.id);auto saved=controller.config.monitors.find(d.id);
    std::wstring text=DisplayLabel(d,controller.config,displays)+L"\r\nLocal identity: "+Wide(d.id)+L"\r\nWindows source: "+Wide(d.alias);
    auto connector=d.technology==5?L"HDMI":d.technology==10?L"DisplayPort external":d.technology==11?L"DisplayPort embedded":d.technology==4?L"DVI":d.technology==0?L"VGA":d.technology==15?L"Miracast":L"unknown / other";
    text+=L"\r\nOS connection: "+std::wstring(connector)+L" (technology "+std::to_wstring(d.technology)+L"); target "+(d.targetAvailable?L"available":L"availability unknown / unavailable");
    text+=L"\r\nOS rotation: "+std::wstring(d.rotation==1?L"0°":d.rotation==2?L"90°":d.rotation==3?L"180°":d.rotation==4?L"270°":L"unknown");
    text+=L"\r\nDesktop pixel bounds: ("+std::to_wstring(d.rect.left)+L", "+std::to_wstring(d.rect.top)+L") — ("+std::to_wstring(d.rect.right)+L", "+std::to_wstring(d.rect.bottom)+L")";
    auto width=static_cast<int64_t>(d.rect.right)-d.rect.left,height=static_cast<int64_t>(d.rect.bottom)-d.rect.top;
    text+=L"\r\nSize / orientation: "+std::to_wstring(width)+L" × "+std::to_wstring(height)+(height>width?L" portrait":L" landscape");
    text+=L"\r\nEligibility: "+std::wstring(!d.identified?L"unknown identity or cloned logical source; independent actions disabled":!p.enabled?L"disabled by saved/effective policy":L"identified and enabled");
    text+=L"\r\nProtection: "+FlyoutPresentation(d)+L"\r\nPrimary reason / deadline: "+ExplanationText(d.id);
    text+=L"\r\nSaved hardware opt-in: "+std::wstring(saved!=controller.config.monitors.end()&&saved->second.hardware?L"yes (permission only)":L"no");
    text+=L"\r\nEffective hardware permission: "+std::wstring(p.hardware?L"yes (permission only)":L"no");
    text+=L"\r\nDDC compatibility: not tested; unknown. Live capability probing is deferred. No supported claim is made from permissions or synthetic tests.";
    text+=L"\r\nObserved hardware state: unknown (no live hardware query).";
    auto status=powerStatus.find(d.id);if(status!=powerStatus.end())text+=L"\r\nCached helper outcome: "+status->second;
    text+=L"\r\nFault / quarantine cache: "+Wide(FaultCacheState(d.id));
    text+=L"\r\nFault observation: "+std::wstring(!faultObservedAt?L"unavailable":now<faultObservedAt||now-faultObservedAt>10000?L"stale":L"cached")+L"; pending recovery reset: "+(FaultResetPending(d.id)?L"yes":L"no");
    bool pending=false;for(auto& task:powerTasks)if(task->id==d.id&&!task->done)pending=true;
    text+=L"\r\nHardware operation / recovery pending: "+std::wstring(pending?L"yes":L"no cached active owner");
    auto lease=controller.leases.find(d.id);if(lease!=controller.leases.end())text+=L"\r\nKeep-awake remaining: "+std::to_wstring(lease->second.deadline>now?(lease->second.deadline-now+999)/1000:0)+L" s";
    if(controller.snoozeUntil>now)text+=L"\r\nGlobal snooze remaining: "+std::to_wstring((controller.snoozeUntil-now+999)/1000)+L" s";
    auto& rules=controller.PolicyNow().rules;for(size_t i=0;i<rules.size();++i){auto& r=rules[i];if(r.kind==2&&(r.global||r.targets.contains(d.id))){
        text+=L"\r\nBackground rule: "+Wide(r.executable)+L" — "+(!r.enabled?L"disabled":controller.background.Uncertain(i,now)?controller.background.Active(i,now)?L"unknown/stale; prior match grace (maximum 5 s)":L"unknown/stale; no inhibitor":controller.background.Active(i,now)?L"confirmed running":L"confirmed absent");}}
    return text;
}
void InspectorSetText(int id,const std::wstring& text) {
    if(ControlText(inspector,id)==text)return;auto child=GetDlgItem(inspector,id);DWORD a=0,b=0;
    auto line=SendMessageW(child,EM_GETFIRSTVISIBLELINE,0,0);SendMessageW(child,EM_GETSEL,reinterpret_cast<WPARAM>(&a),reinterpret_cast<LPARAM>(&b));
    SetWindowTextW(child,text.c_str());if(id==InspectorDetail){SendMessageW(child,EM_SETSEL,std::min<size_t>(a,text.size()),std::min<size_t>(b,text.size()));auto current=SendMessageW(child,EM_GETFIRSTVISIBLELINE,0,0);SendMessageW(child,EM_LINESCROLL,0,line-current);}
}
void InspectorRefresh() {
    if(!inspector)return;auto now=GetTickCount64();auto list=GetDlgItem(inspector,InspectorList);
    std::vector<std::string> ids;std::vector<std::wstring> rows;for(auto& d:displays){ids.push_back(d.id);rows.push_back(DisplayLabel(d,controller.config,displays));}
    if(inspectorSelected.empty()&&!ids.empty())inspectorSelected=ids.front();
    if(ids!=inspectorIds||rows!=inspectorRows){int top=static_cast<int>(SendMessageW(list,LB_GETTOPINDEX,0,0));std::string topId=top>=0&&top<static_cast<int>(inspectorIds.size())?inspectorIds[top]:"";
        SendMessageW(list,WM_SETREDRAW,FALSE,0);SendMessageW(list,LB_RESETCONTENT,0,0);int selected=-1;
        for(size_t i=0;i<ids.size();++i){SendMessageW(list,LB_ADDSTRING,0,reinterpret_cast<LPARAM>(rows[i].c_str()));if(ids[i]==inspectorSelected)selected=static_cast<int>(i);if(ids[i]==topId)top=static_cast<int>(i);}
        SendMessageW(list,LB_SETCURSEL,selected,0);if(!ids.empty())SendMessageW(list,LB_SETTOPINDEX,std::clamp(top,0,static_cast<int>(ids.size())-1),0);SendMessageW(list,WM_SETREDRAW,TRUE,0);InvalidateRect(list,nullptr,TRUE);
    }inspectorIds=std::move(ids);inspectorRows=std::move(rows);
    int current=-1;for(size_t i=0;i<inspectorIds.size();++i)if(inspectorIds[i]==inspectorSelected)current=static_cast<int>(i);
    if(SendMessageW(list,LB_GETCURSEL,0,0)!=current)SendMessageW(list,LB_SETCURSEL,current,0);
    const Display* selected=nullptr;for(auto& d:displays)if(d.id==inspectorSelected)selected=&d;
    InspectorSetText(InspectorDetail,selected?InspectorText(*selected,now):L"Selected display disconnected or unavailable. Select a connected display.");
    std::wstring status=displaySnapshot.report+L"\r\nTopology generation "+std::to_wstring(displaySnapshot.topology)+L"; sampled at monotonic "+std::to_wstring(displaySnapshot.sampled)+L" ms; "+(SnapshotStale(now)?L"stale / unknown":L"cached current (OS only)");
    InspectorSetText(InspectorStatus,status);EnableWindow(GetDlgItem(inspector,InspectorRescan),!displayScanBusy);
    for(int id:{InspectorSettings,InspectorControls})EnableWindow(GetDlgItem(inspector,id),selected&&!topologyPending);
}
void LayoutInspector(){if(!inspector)return;RECT r{};GetClientRect(inspector,&r);auto width=std::max(1,MulDiv(r.right,96,inspectorLayout.dpi)-32);for(auto& c:inspectorLayout.controls)c.width=width;LayoutSettings(inspector,inspectorLayout,940,inspectorScroll);}
void RefreshInspectorTheme(){if(!inspector)return;if(auto font=SettingsFont(inspectorLayout.dpi)){for(auto& c:inspectorLayout.controls)SendMessageW(c.window,WM_SETFONT,reinterpret_cast<WPARAM>(font),FALSE);if(inspectorFont)DeleteObject(inspectorFont);inspectorFont=font;}ThemeChrome(inspector);RedrawWindow(inspector,nullptr,nullptr,RDW_INVALIDATE|RDW_ERASE|RDW_ALLCHILDREN);}
void InspectorReveal(HWND child){RECT client{},r{};GetClientRect(inspector,&client);GetWindowRect(child,&r);MapWindowPoints(nullptr,inspector,reinterpret_cast<POINT*>(&r),2);int delta=r.top<0?r.top:r.bottom>client.bottom?r.bottom-client.bottom:0;if(delta){inspectorScroll+=delta;LayoutInspector();}}
LRESULT CALLBACK InspectorProc(HWND w,UINT msg,WPARAM wp,LPARAM lp) {
    LRESULT result{};if(ThemeMessage(w,msg,wp,lp,result))return result;
    if(msg==WM_SIZE){LayoutInspector();return 0;}if(ScrollSettings(w,msg,wp,lp,940,inspectorScroll,inspectorLayout))return 0;
    if(msg==WM_TIMER){InspectorRefresh();return 0;}
    if(msg==WM_DPICHANGED){SettingsDpiChanged(w,wp,lp,inspectorLayout,inspectorFont,940,inspectorScroll);LayoutInspector();return 0;}
    if(msg==WM_COMMAND){int id=LOWORD(wp),event=HIWORD(wp);
        if(event==EN_SETFOCUS||event==BN_SETFOCUS||(id==InspectorList&&event==LBN_SETFOCUS)){InspectorReveal(reinterpret_cast<HWND>(lp));return 0;}
        if(id==InspectorList&&event==LBN_SELCHANGE){auto i=static_cast<int>(SendMessageW(GetDlgItem(w,id),LB_GETCURSEL,0,0));if(i>=0&&i<static_cast<int>(inspectorIds.size()))inspectorSelected=inspectorIds[i];InspectorRefresh();return 0;}
        if(event!=BN_CLICKED)return 0;
        if(id==InspectorRescan){RequestDisplayScan();InspectorRefresh();}else if(id==InspectorExport)ExportDiagnostics(w);else if(id==InspectorMap)ShowDisplayMap();
        else if(id==InspectorSettings)ShowDisplaySettings(inspectorSelected);else if(id==InspectorControls){if(!topologyPending&&std::any_of(displays.begin(),displays.end(),[](const Display& d){return d.id==inspectorSelected;})){flyoutSelected=inspectorSelected;ShowFlyout();}}
        else if(id==IDCANCEL)DestroyWindow(w);return 0;
    }
    if(msg==WM_CLOSE){DestroyWindow(w);return 0;}if(msg==WM_DESTROY){KillTimer(w,1);{std::lock_guard lock(registryLock);ownedWindows.erase(w);}inspector=nullptr;return 0;}
    if(msg==WM_NCDESTROY){inspectorLayout.controls.clear();inspectorIds.clear();inspectorRows.clear();if(inspectorFont){
#ifdef DAC_HARNESS
        inspectorFontDeleted=DeleteObject(inspectorFont);
#else
        DeleteObject(inspectorFont);
#endif
        inspectorFont=nullptr;}}
    return DefWindowProcW(w,msg,wp,lp);
}
void ShowInspector(const std::string& id) {
    if(!id.empty())inspectorSelected=id;if(inspector){InspectorRefresh();
#ifndef DAC_HARNESS
        ShowWindow(inspector,SW_SHOWNORMAL);SetForegroundWindow(inspector);
#endif
        return;}
    if(!inspectorRegistered){WNDCLASSW wc{};wc.hInstance=GetModuleHandleW(nullptr);wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);wc.lpfnWndProc=InspectorProc;wc.lpszClassName=kInspector;if(!RegisterClassW(&wc))return;inspectorRegistered=true;}
    inspectorLayout={};inspectorScroll=0;inspector=CreateWindowExW(WS_EX_TOOLWINDOW|WS_EX_CONTROLPARENT,kInspector,L"DAC — Display inspector (local cached details)",WS_CAPTION|WS_SYSMENU|WS_THICKFRAME|WS_VSCROLL|WS_CLIPCHILDREN,CW_USEDEFAULT,CW_USEDEFAULT,850,800,ui,nullptr,GetModuleHandleW(nullptr),nullptr);if(!inspector)return;
    {std::lock_guard lock(registryLock);ownedWindows.insert(inspector);}FitSettings(inspector,850,800);inspectorLayout.dpi=GetDpiForWindow(inspector);RefreshInspectorTheme();
    auto control=[&](const wchar_t* cls,const wchar_t* text,DWORD style,int y,int height,int controlId){auto child=CreateWindowExW(0,cls,text,WS_CHILD|WS_VISIBLE|style,0,0,1,1,inspector,reinterpret_cast<HMENU>(static_cast<INT_PTR>(controlId)),GetModuleHandleW(nullptr),nullptr);inspectorLayout.controls.push_back({child,16,y,800,height});ThemeControl(child,cls);SendMessageW(child,WM_SETFONT,reinterpret_cast<WPARAM>(inspectorFont),TRUE);};
    control(L"STATIC",L"Connected &displays — local identity details; exports are redacted",0,12,28,0);
    control(L"LISTBOX",L"",WS_BORDER|WS_TABSTOP|LBS_NOTIFY|WS_VSCROLL,44,100,InspectorList);
    control(L"STATIC",L"",SS_NOPREFIX,154,64,InspectorStatus);
    control(L"STATIC",L"Selected display &details (read only)",0,222,24,0);
    control(L"EDIT",L"",WS_BORDER|WS_TABSTOP|WS_VSCROLL|ES_MULTILINE|ES_READONLY|ES_AUTOVSCROLL,252,432,InspectorDetail);
    control(L"BUTTON",L"&Rescan Windows display information only",BS_PUSHBUTTON|BS_NOTIFY|WS_TABSTOP,696,32,InspectorRescan);
    control(L"BUTTON",L"&Export redacted diagnostics…",BS_PUSHBUTTON|BS_NOTIFY|WS_TABSTOP,736,32,InspectorExport);
    control(L"BUTTON",L"Selected display &settings…",BS_PUSHBUTTON|BS_NOTIFY|WS_TABSTOP,776,32,InspectorSettings);
    control(L"BUTTON",L"Display &map…",BS_PUSHBUTTON|BS_NOTIFY|WS_TABSTOP,816,32,InspectorMap);
    control(L"BUTTON",L"Display &controls…",BS_PUSHBUTTON|BS_NOTIFY|WS_TABSTOP,856,32,InspectorControls);
    control(L"BUTTON",L"&Close",BS_PUSHBUTTON|BS_NOTIFY|WS_TABSTOP,896,32,IDCANCEL);
    InspectorRefresh();LayoutInspector();SetTimer(inspector,1,1000,nullptr);
#ifndef DAC_HARNESS
    ShowWindow(inspector,SW_SHOWNORMAL);SetForegroundWindow(inspector);SetFocus(GetDlgItem(inspector,InspectorList));
#endif
}
