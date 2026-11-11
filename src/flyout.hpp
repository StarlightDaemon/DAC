// SPDX-License-Identifier: MIT
// Copyright (c) 2026 StarlightDaemon
#pragma once

// Included by platform.hpp inside namespace dac, after the guarded display
// commands. This surface reads UI-owner snapshots and never probes hardware.
constexpr wchar_t kFlyout[]=L"DAC-DisplayFlyout";
constexpr int kFlyoutHeight=768,kFlyoutWidth=664;
enum FlyoutControl {FlyoutList=701,FlyoutDetail,FlyoutStart,FlyoutStop,
    Flyout5,Flyout15,Flyout30,Flyout60,FlyoutMinutes,FlyoutCustom,FlyoutUntilText,
    FlyoutUntil,FlyoutCancel,FlyoutError,FlyoutSettings,FlyoutMap,FlyoutInspector};
HWND flyout{};
HFONT flyoutFont{};
SettingsLayout flyoutLayout;
std::vector<SettingsControl> flyoutBaseControls;
bool flyoutRegistered=false;
int flyoutScroll=0;
Time flyoutTopology=0;
std::string flyoutSelected;
std::vector<std::string> flyoutIds;
std::vector<std::wstring> flyoutRows;
#ifdef DAC_HARNESS
BOOL flyoutFontDeleteResult=TRUE;
DWORD flyoutFontDeleteError=0;
#endif

int FlyoutLineHeight() {
    int height=MulDiv(21,flyoutLayout.dpi,96);
    if(flyout){HDC dc=GetDC(flyout);if(dc){auto old=flyoutFont?SelectObject(dc,flyoutFont):nullptr;TEXTMETRICW metrics{};if(GetTextMetricsW(dc,&metrics))height=std::max(height,static_cast<int>(metrics.tmHeight+metrics.tmExternalLeading));if(old)SelectObject(dc,old);ReleaseDC(flyout,dc);}}
    return height;
}
int FlyoutRowHeight(){return std::max(MulDiv(74,flyoutLayout.dpi,96),FlyoutLineHeight()*3+MulDiv(10,flyoutLayout.dpi,96));}
void LayoutFlyout() {
    RECT client{};GetClientRect(flyout,&client);
    int content=std::max(1,MulDiv(client.right,96,flyoutLayout.dpi)-32);
    for(size_t i=0;i<flyoutLayout.controls.size()&&i<flyoutBaseControls.size();++i) {
        auto& current=flyoutLayout.controls[i];auto& base=flyoutBaseControls[i];
        current.x=16+MulDiv(base.x-16,content,610);current.width=std::max(1,MulDiv(base.width,content,610));
    }
    LayoutSettings(flyout,flyoutLayout,kFlyoutHeight,flyoutScroll);
}
void ClampFlyoutWidth() {
    RECT bounds{};GetWindowRect(flyout,&bounds);MONITORINFO monitor{};monitor.cbSize=sizeof(monitor);
    if(!GetMonitorInfoW(MonitorFromWindow(flyout,MONITOR_DEFAULTTONEAREST),&monitor))return;
    int width=std::min(bounds.right-bounds.left,monitor.rcWork.right-monitor.rcWork.left);
    int left=std::max(monitor.rcWork.left,std::min(bounds.left,monitor.rcWork.right-width));
    SetWindowPos(flyout,nullptr,left,bounds.top,width,bounds.bottom-bounds.top,SWP_NOZORDER|SWP_NOACTIVATE);
}
void FlyoutReveal(HWND child) {
    RECT client{},bounds{};GetClientRect(flyout,&client);GetWindowRect(child,&bounds);MapWindowPoints(nullptr,flyout,reinterpret_cast<POINT*>(&bounds),2);
    int delta=bounds.top<0?bounds.top:bounds.bottom>client.bottom?bounds.bottom-client.bottom:0;
    if(!delta)return;
    SCROLLINFO info{};info.cbSize=sizeof(info);info.fMask=SIF_POS;info.nPos=flyoutScroll+delta;SetScrollInfo(flyout,SB_VERT,&info,TRUE);GetScrollInfo(flyout,SB_VERT,&info);
    ScrollWindowEx(flyout,0,flyoutScroll-info.nPos,nullptr,nullptr,nullptr,nullptr,SW_SCROLLCHILDREN|SW_INVALIDATE|SW_ERASE);flyoutScroll=info.nPos;
}
bool ShowDisplaySettings(const std::string& id) {
    if(topologyPending||std::none_of(displays.begin(),displays.end(),[&](const Display& display){return display.id==id;}))return false;
    if(setupWindow){SetForegroundWindow(setupWindow);return false;}
    if(editor) {
        if(!IsWindowEnabled(editor)){SetForegroundWindow(GetLastActivePopup(editor));return false;}
        if(!ValidMonitorNumbers()||!StoreGlobalDraft()){SetForegroundWindow(editor);return false;}
    }
    ShowSettings();if(!editor)return false;
    int index=-1;for(size_t i=0;i<editorDisplays.size();++i)if(editorDisplays[i].id==id)index=static_cast<int>(i);
    if(index<0) {
        editorDisplays=displays;SendDlgItemMessageW(editor,120,CB_RESETCONTENT,0,0);
        for(size_t i=0;i<editorDisplays.size();++i){auto label=DisplayLabel(editorDisplays[i],draft,editorDisplays);SendDlgItemMessageW(editor,120,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(label.c_str()));if(editorDisplays[i].id==id)index=static_cast<int>(i);}
    }
    if(index<0)return false;
    SendDlgItemMessageW(editor,120,CB_SETCURSEL,index,0);LoadMonitorDraft();editorScroll=MulDiv(276,editorLayout.dpi,96);LayoutSettings(editor,editorLayout,1240,editorScroll);
#ifndef DAC_HARNESS
    ShowWindow(editor,SW_SHOWNORMAL);SetForegroundWindow(editor);SetFocus(GetDlgItem(editor,120));
#endif
    return true;
}

std::wstring FlyoutPresentation(const Display& display) {
    auto it=controller.nodes.find(display.id);
    if(!display.identified)return L"Identity unavailable";
    if(!controller.Pref(display.id).enabled)return L"Disabled";
    if(controller.Spanning())return L"Desktop-spanning presentation active";
    if(it==controller.nodes.end())return L"Display state unavailable";
    auto& node=it->second;
    switch(node.state) {
    case State::Disabled:return L"Disabled";
    case State::Desktop:return L"Desktop";
    case State::Suppressed:return L"Desktop — activation inhibited";
    case State::Black:return L"Black";
    case State::Launching:case State::Saver:{int presentation=node.presentation==-2?controller.Pref(display.id).saver:node.presentation;
        const wchar_t* labels[]={L"Bubbles",L"Mystify",L"Ribbons",L"3D text",L"Windows photos",L"Blank saver",L"Custom saver",L"Photo slideshow",L"Moving dim clock",L"Sparse constellation"};
        auto name=presentation>=0&&presentation<static_cast<int>(std::size(labels))?labels[presentation]:L"presentation";
        return std::wstring(node.state==State::Launching?L"Starting: ":L"Active: ")+name;}
    case State::Fallback:return L"Black fallback";
    case State::Dim:return L"Software dim warning";
    }
    return L"Display state unavailable";
}
std::wstring FlyoutRow(const Display& display,Time now) {
    auto reason=topologyPending?L"Display topology is being refreshed":ExplanationText(display.id);
    auto lease=controller.leases.find(display.id);
    if(lease!=controller.leases.end()) {
        Time remaining=lease->second.deadline>now?lease->second.deadline-now:0;
        reason+=L"; keep awake "+std::to_wstring((remaining+999)/1000)+L" s";
        if(!lease->second.resolvedExpiry.empty())reason+=L" (until "+Wide(lease->second.resolvedExpiry)+L")";
    }
    return DisplayLabel(display,controller.config,displays)+L"\r\n"+FlyoutPresentation(display)+L"\r\n"+reason;
}
void FlyoutSetText(int id,const std::wstring& text) {
    // Avoid repeated accessibility notifications and caret/selection changes.
    if(ControlText(flyout,id)==text)return;
    HWND control=GetDlgItem(flyout,id);DWORD first=0,last=0;
    bool reading=id==FlyoutDetail&&GetFocus()==control;
    if(reading)SendMessageW(control,EM_GETSEL,reinterpret_cast<WPARAM>(&first),reinterpret_cast<LPARAM>(&last));
    SetWindowTextW(control,text.c_str());
    if(reading)SendMessageW(control,EM_SETSEL,std::min<size_t>(first,text.size()),std::min<size_t>(last,text.size()));
}
void FlyoutRefresh() {
    if(!flyout)return;
    HWND list=GetDlgItem(flyout,FlyoutList);if(!list)return;
    Time now=GetTickCount64();
    int top=static_cast<int>(SendMessageW(list,LB_GETTOPINDEX,0,0));
    std::string topId=top>=0&&top<static_cast<int>(flyoutIds.size())?flyoutIds[top]:"";
    std::vector<std::string> ids;std::vector<std::wstring> rows;
    for(auto& display:displays){ids.push_back(display.id);rows.push_back(FlyoutRow(display,now));}
    if(flyoutSelected.empty()&&!ids.empty())flyoutSelected=ids.front();
    bool changed=ids!=flyoutIds||rows!=flyoutRows;
    if(changed) {
        // HWNDs and focus survive refresh. The strings retained by the native
        // list box expose the same complete text as the multiline painter.
        SendMessageW(list,WM_SETREDRAW,FALSE,0);
        bool topologyChanged=ids!=flyoutIds;
        flyoutIds=std::move(ids);flyoutRows=std::move(rows);
        if(topologyChanged) {
            SendMessageW(list,LB_RESETCONTENT,0,0);
            for(auto& row:flyoutRows)SendMessageW(list,LB_ADDSTRING,0,reinterpret_cast<LPARAM>(row.c_str()));
        }else for(size_t i=0;i<flyoutRows.size();++i) {
            int length=static_cast<int>(SendMessageW(list,LB_GETTEXTLEN,i,0));
            std::wstring old(length>=0?static_cast<size_t>(length)+1:1,L'\0');
            if(length>=0)SendMessageW(list,LB_GETTEXT,i,reinterpret_cast<LPARAM>(old.data()));
            if(length>=0)old.resize(length);
            if(old!=flyoutRows[i]){SendMessageW(list,LB_DELETESTRING,i,0);SendMessageW(list,LB_INSERTSTRING,i,reinterpret_cast<LPARAM>(flyoutRows[i].c_str()));}
        }
        int selected=-1;for(size_t i=0;i<flyoutIds.size();++i)if(flyoutIds[i]==flyoutSelected)selected=static_cast<int>(i);
        SendMessageW(list,LB_SETCURSEL,static_cast<WPARAM>(selected),0);
        if(!topId.empty())for(size_t i=0;i<flyoutIds.size();++i)if(flyoutIds[i]==topId){top=static_cast<int>(i);break;}
        if(!flyoutIds.empty())SendMessageW(list,LB_SETTOPINDEX,std::clamp(top,0,static_cast<int>(flyoutIds.size())-1),0);
        SendMessageW(list,WM_SETREDRAW,TRUE,0);InvalidateRect(list,nullptr,FALSE);
    }
    flyoutTopology=topologyEpoch;
    int currentSelection=-1;for(size_t i=0;i<flyoutIds.size();++i)if(flyoutIds[i]==flyoutSelected)currentSelection=static_cast<int>(i);
    if(SendMessageW(list,LB_GETCURSEL,0,0)!=currentSelection)SendMessageW(list,LB_SETCURSEL,currentSelection,0);
    const Display* selected=nullptr;for(auto& display:displays)if(display.id==flyoutSelected){selected=&display;break;}
    std::wstring detail;
    if(selected)detail=FlyoutRow(*selected,now);
    else detail=displays.empty()?L"No connected displays.":L"Selected display disconnected. Select a connected display to continue.";
    bool eligible=selected&&!topologyPending&&selected->identified&&controller.nodes.contains(selected->id)&&controller.Pref(selected->id).enabled&&!controller.Spanning();
    bool start=eligible&&!controller.blocked&&!controller.paused;
    bool lease=eligible&&controller.LeaseRejection(flyoutSelected).empty();
    if(selected) {
        if(controller.Spanning())detail+=L"\r\nStop the desktop span from the tray before using individual display controls.";
        else if(!selected->identified)detail+=L"\r\nActions require a stable display identity.";
        else if(!controller.Pref(selected->id).enabled)detail+=L"\r\nEnable this display in Settings to use its controls.";
        else if(topologyPending)detail+=L"\r\nActions are unavailable while the display list refreshes.";
        else if(!lease){auto rejection=controller.LeaseRejection(flyoutSelected);if(!rejection.empty())detail+=L"\r\nKeep awake: "+Wide(rejection);}
    }
    FlyoutSetText(FlyoutDetail,detail);
    EnableWindow(GetDlgItem(flyout,FlyoutStart),start);
    bool running=eligible&&Running(controller.nodes.at(flyoutSelected).state);
    EnableWindow(GetDlgItem(flyout,FlyoutStop),running);
    for(int id:{Flyout5,Flyout15,Flyout30,Flyout60,FlyoutMinutes,FlyoutCustom,FlyoutUntilText,FlyoutUntil})EnableWindow(GetDlgItem(flyout,id),lease);
    EnableWindow(GetDlgItem(flyout,FlyoutCancel),eligible&&controller.leases.contains(flyoutSelected));
    EnableWindow(GetDlgItem(flyout,FlyoutSettings),selected&&!topologyPending);
}
void RefreshFlyoutTheme() {
    if(!flyout)return;
    HFONT replacement=SettingsFont(flyoutLayout.dpi);
    if(replacement){for(auto& control:flyoutLayout.controls)SendMessageW(control.window,WM_SETFONT,reinterpret_cast<WPARAM>(replacement),FALSE);if(flyoutFont)DeleteObject(flyoutFont);flyoutFont=replacement;}
    SendDlgItemMessageW(flyout,FlyoutList,LB_SETITEMHEIGHT,0,FlyoutRowHeight());
    ThemeChrome(flyout);RedrawWindow(flyout,nullptr,nullptr,RDW_INVALIDATE|RDW_ERASE|RDW_ALLCHILDREN);
}
bool FlyoutDrawItem(const DRAWITEMSTRUCT& draw) {
    if(draw.CtlType!=ODT_LISTBOX||draw.CtlID!=FlyoutList)return false;
    int saved=SaveDC(draw.hDC);bool selected=(draw.itemState&ODS_SELECTED)!=0;
    COLORREF background=selected?(theme.highContrast?GetSysColor(COLOR_HIGHLIGHT):theme.colors.accent):theme.colors.surface;
    COLORREF foreground=selected?(theme.highContrast?GetSysColor(COLOR_HIGHLIGHTTEXT):theme.colors.onAccent):theme.colors.text;
    FillColor(draw.hDC,draw.rcItem,background);SetBkMode(draw.hDC,TRANSPARENT);SetTextColor(draw.hDC,foreground);
    if(flyoutFont)SelectObject(draw.hDC,flyoutFont);
    if(draw.itemID<flyoutRows.size()) {
        auto& text=flyoutRows[draw.itemID];size_t start=0;int line=0;
        int dpi=flyoutLayout.dpi,pad=MulDiv(8,dpi,96),lineHeight=FlyoutLineHeight();
        while(start<text.size()&&line<3){auto end=text.find(L"\r\n",start);if(end==std::wstring::npos)end=text.size();
            RECT bounds=draw.rcItem;bounds.left+=pad;bounds.right-=pad;bounds.top+=MulDiv(5,dpi,96)+line*lineHeight;bounds.bottom=bounds.top+lineHeight;
            DrawTextW(draw.hDC,text.c_str()+start,static_cast<int>(end-start),&bounds,DT_SINGLELINE|DT_NOPREFIX|DT_END_ELLIPSIS|DT_VCENTER);
            start=end==text.size()?end:end+2;++line;
        }
    }
    if((draw.itemState&ODS_FOCUS)&&!(draw.itemState&ODS_NOFOCUSRECT))DrawFocusRect(draw.hDC,&draw.rcItem);
    RestoreDC(draw.hDC,saved);return true;
}
LRESULT CALLBACK FlyoutProc(HWND w,UINT msg,WPARAM wp,LPARAM lp) {
    if(msg==WM_DRAWITEM&&FlyoutDrawItem(*reinterpret_cast<DRAWITEMSTRUCT*>(lp)))return TRUE;
    if(msg==WM_MEASUREITEM){auto measure=reinterpret_cast<MEASUREITEMSTRUCT*>(lp);if(measure->CtlType==ODT_LISTBOX&&measure->CtlID==FlyoutList){measure->itemHeight=FlyoutRowHeight();return TRUE;}}
    LRESULT themed{};if(ThemeMessage(w,msg,wp,lp,themed))return themed;
    if(msg==WM_SIZE){if(flyout)LayoutFlyout();return 0;}
    if(ScrollSettings(w,msg,wp,lp,kFlyoutHeight,flyoutScroll,flyoutLayout))return 0;
    if(msg==WM_TIMER){if(wp==1)FlyoutRefresh();return 0;}
    if(msg==WM_COMMAND) {
        int id=LOWORD(wp);
        if(HIWORD(wp)==EN_SETFOCUS||HIWORD(wp)==BN_SETFOCUS||(id==FlyoutList&&HIWORD(wp)==LBN_SETFOCUS)){FlyoutReveal(reinterpret_cast<HWND>(lp));return 0;}
        if(id==IDCANCEL&&HIWORD(wp)==BN_CLICKED){DestroyWindow(w);return 0;}
        if(id==FlyoutMap&&HIWORD(wp)==BN_CLICKED){ShowDisplayMap();return 0;}
        if(id==FlyoutInspector&&HIWORD(wp)==BN_CLICKED){ShowInspector(flyoutSelected);return 0;}
        if(id==FlyoutList&&HIWORD(wp)==LBN_SELCHANGE){int index=static_cast<int>(SendDlgItemMessageW(w,FlyoutList,LB_GETCURSEL,0,0));if(index>=0&&index<static_cast<int>(flyoutIds.size()))flyoutSelected=flyoutIds[index];FlyoutSetText(FlyoutError,L"");FlyoutRefresh();return 0;}
        if(HIWORD(wp)!=BN_CLICKED)return 0;
        if(id==FlyoutSettings){if(!ShowDisplaySettings(flyoutSelected))FlyoutSetText(FlyoutError,L"Finish the open settings dialog or correct its draft before selecting this display.");return 0;}
        bool action=id==FlyoutStart||id==FlyoutStop||id==Flyout5||id==Flyout15||id==Flyout30||id==Flyout60||id==FlyoutCustom||id==FlyoutUntil||id==FlyoutCancel;
        if(!action)return 0;
        std::wstring error;bool ok=false;
        if(id==FlyoutUntil)ok=ApplyDisplayUntil(flyoutSelected,flyoutTopology,ControlText(w,FlyoutUntilText),error);
        else {
            int minutes=id==Flyout5?5:id==Flyout15?15:id==Flyout30?30:id==Flyout60?60:0;
            if(id==FlyoutCustom){BOOL valid=FALSE;UINT value=GetDlgItemInt(w,FlyoutMinutes,&valid,FALSE);if(!valid||value<1||value>1440)error=L"Enter a whole number of minutes from 1 to 1440.";else minutes=static_cast<int>(value);}
            if(error.empty())ok=ApplyDisplayControl(flyoutSelected,flyoutTopology,id==FlyoutStart?0:id==FlyoutStop?1:id==FlyoutCancel?3:2,minutes);
        }
        if(!ok&&error.empty())error=L"The display or its policy changed. Review its current status and try again.";
        FlyoutSetText(FlyoutError,error);FlyoutRefresh();return 0;
    }
    if(msg==WM_DPICHANGED){SettingsDpiChanged(w,wp,lp,flyoutLayout,flyoutFont,kFlyoutHeight,flyoutScroll);ClampFlyoutWidth();LayoutFlyout();SendDlgItemMessageW(w,FlyoutList,LB_SETITEMHEIGHT,0,FlyoutRowHeight());return 0;}
    if(msg==WM_CLOSE){DestroyWindow(w);return 0;}
    if(msg==WM_DESTROY){KillTimer(w,1);{std::lock_guard lock(registryLock);ownedWindows.erase(w);}flyout=nullptr;return 0;}
    if(msg==WM_NCDESTROY){flyoutLayout.controls.clear();flyoutBaseControls.clear();flyoutIds.clear();flyoutRows.clear();if(flyoutFont){
#ifdef DAC_HARNESS
        SetLastError(0);flyoutFontDeleteResult=DeleteObject(flyoutFont);flyoutFontDeleteError=GetLastError();
#else
        DeleteObject(flyoutFont);
#endif
        flyoutFont=nullptr;}}
    return DefWindowProcW(w,msg,wp,lp);
}
void ShowFlyout() {
    if(flyout){FlyoutRefresh();
#ifndef DAC_HARNESS
        ShowWindow(flyout,SW_SHOWNORMAL);SetForegroundWindow(flyout);
#endif
        return;}
    if(!flyoutRegistered){WNDCLASSW wc{};wc.lpfnWndProc=FlyoutProc;wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=kFlyout;wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);if(!RegisterClassW(&wc))return;flyoutRegistered=true;}
    flyoutLayout={};flyoutScroll=0;
    flyout=CreateWindowExW(WS_EX_TOOLWINDOW|WS_EX_CONTROLPARENT,kFlyout,L"DAC — Connected display controls",WS_CAPTION|WS_SYSMENU|WS_VSCROLL|WS_CLIPCHILDREN,CW_USEDEFAULT,CW_USEDEFAULT,kFlyoutWidth,810,ui,nullptr,GetModuleHandleW(nullptr),nullptr);
    if(!flyout)return;
    {std::lock_guard lock(registryLock);ownedWindows.insert(flyout);}
    POINT cursor{};GetCursorPos(&cursor);MONITORINFO monitor{};monitor.cbSize=sizeof(monitor);
    if(GetMonitorInfoW(MonitorFromPoint(cursor,MONITOR_DEFAULTTONEAREST),&monitor))SetWindowPos(flyout,nullptr,monitor.rcWork.left,monitor.rcWork.top,0,0,SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE);
    FitSettings(flyout,kFlyoutWidth,810);flyoutLayout.dpi=GetDpiForWindow(flyout);
    // Moving the new window to the cursor's monitor can synchronously deliver
    // WM_DPICHANGED before any children exist and already install a font.
    if(auto initialFont=SettingsFont(flyoutLayout.dpi)){if(flyoutFont)DeleteObject(flyoutFont);flyoutFont=initialFont;}ThemeChrome(flyout);
    RECT bounds{};GetWindowRect(flyout,&bounds);if(monitor.rcWork.right>monitor.rcWork.left){int width=bounds.right-bounds.left,height=bounds.bottom-bounds.top;SetWindowPos(flyout,nullptr,std::max(monitor.rcWork.left,std::min(cursor.x-width/2,monitor.rcWork.right-width)),std::max(monitor.rcWork.top,std::min(cursor.y-height,monitor.rcWork.bottom-height)),0,0,SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE);}
    auto control=[&](const wchar_t* cls,const wchar_t* text,DWORD style,int x,int y,int width,int height,int id){if(wcscmp(cls,L"BUTTON")==0)style|=BS_NOTIFY;auto child=CreateWindowExW(0,cls,text,WS_CHILD|WS_VISIBLE|style,MulDiv(x,flyoutLayout.dpi,96),MulDiv(y,flyoutLayout.dpi,96),MulDiv(width,flyoutLayout.dpi,96),MulDiv(height,flyoutLayout.dpi,96),flyout,reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),GetModuleHandleW(nullptr),nullptr);flyoutLayout.controls.push_back({child,x,y,width,height});ThemeControl(child,cls);SendMessageW(child,WM_SETFONT,reinterpret_cast<WPARAM>(flyoutFont?flyoutFont:GetStockObject(DEFAULT_GUI_FONT)),TRUE);return child;};
    control(L"STATIC",L"Connected &displays",0,16,16,610,24,0);
    control(L"LISTBOX",L"",WS_BORDER|WS_TABSTOP|WS_VSCROLL|LBS_NOTIFY|LBS_NOINTEGRALHEIGHT|LBS_OWNERDRAWFIXED|LBS_HASSTRINGS,16,48,610,222,FlyoutList);
    control(L"STATIC",L"Selected display — full name, presentation and reason",SS_NOPREFIX,16,282,610,22,0);
    control(L"EDIT",L"",WS_BORDER|WS_VSCROLL|WS_TABSTOP|ES_MULTILINE|ES_READONLY|ES_AUTOVSCROLL,16,308,610,88,FlyoutDetail);
    control(L"BUTTON",L"&Start",BS_PUSHBUTTON|WS_TABSTOP,16,410,135,32,FlyoutStart);
    control(L"BUTTON",L"S&top",BS_PUSHBUTTON|WS_TABSTOP,163,410,135,32,FlyoutStop);
    control(L"BUTTON",L"&Cancel keep awake",BS_PUSHBUTTON|WS_TABSTOP,310,410,196,32,FlyoutCancel);
    control(L"STATIC",L"Keep the selected display awake in DAC",SS_NOPREFIX,16,454,610,22,0);
    control(L"BUTTON",L"&5 minutes",BS_PUSHBUTTON|WS_TABSTOP,16,484,140,32,Flyout5);
    control(L"BUTTON",L"&15 minutes",BS_PUSHBUTTON|WS_TABSTOP,172,484,140,32,Flyout15);
    control(L"BUTTON",L"&30 minutes",BS_PUSHBUTTON|WS_TABSTOP,328,484,140,32,Flyout30);
    control(L"BUTTON",L"&60 minutes",BS_PUSHBUTTON|WS_TABSTOP,484,484,140,32,Flyout60);
    control(L"STATIC",L"Custom &minutes (1–1440)",0,16,532,230,24,0);
    control(L"EDIT",L"45",WS_BORDER|WS_TABSTOP|ES_NUMBER|ES_AUTOHSCROLL,250,528,92,28,FlyoutMinutes);SendDlgItemMessageW(flyout,FlyoutMinutes,EM_SETLIMITTEXT,4,0);
    control(L"BUTTON",L"&Apply minutes",BS_PUSHBUTTON|WS_TABSTOP,356,526,180,32,FlyoutCustom);
    control(L"STATIC",L"&Until local time",0,16,578,160,24,0);
    control(L"EDIT",L"",WS_BORDER|WS_TABSTOP|ES_AUTOHSCROLL,180,572,266,28,FlyoutUntilText);SendDlgItemMessageW(flyout,FlyoutUntilText,EM_SETLIMITTEXT,16,0);
    control(L"BUTTON",L"Apply e&xpiry",BS_PUSHBUTTON|WS_TABSTOP,458,570,168,32,FlyoutUntil);
    control(L"STATIC",L"Until format: YYYY-MM-DD HH:MM (within 24 hours). Ambiguous or missing local times are rejected. The resolved duration uses a monotonic clock; later clock changes do not move it. Keep awake inhibits DAC activation; Windows may still sleep.",SS_NOPREFIX,16,614,610,76,0);
    control(L"STATIC",L"",SS_NOPREFIX,16,692,610,34,FlyoutError);
    control(L"BUTTON",L"Se&ttings…",BS_PUSHBUTTON|WS_TABSTOP,16,728,140,32,FlyoutSettings);
    control(L"BUTTON",L"&Map…",BS_PUSHBUTTON|WS_TABSTOP,172,728,140,32,FlyoutMap);
    control(L"BUTTON",L"&Inspect…",BS_PUSHBUTTON|WS_TABSTOP,328,728,140,32,FlyoutInspector);
    control(L"BUTTON",L"&Close",BS_PUSHBUTTON|WS_TABSTOP,484,728,140,32,IDCANCEL);
    SendDlgItemMessageW(flyout,FlyoutList,LB_SETITEMHEIGHT,0,FlyoutRowHeight());
    FILETIME utc{},localFile{};GetSystemTimeAsFileTime(&utc);ULARGE_INTEGER future{};future.LowPart=utc.dwLowDateTime;future.HighPart=utc.dwHighDateTime;future.QuadPart+=36000000000ULL;utc.dwLowDateTime=future.LowPart;utc.dwHighDateTime=future.HighPart;SYSTEMTIME local{};FileTimeToLocalFileTime(&utc,&localFile);FileTimeToSystemTime(&localFile,&local);wchar_t until[32]{};swprintf_s(until,L"%04u-%02u-%02u %02u:%02u",local.wYear,local.wMonth,local.wDay,local.wHour,local.wMinute);SetDlgItemTextW(flyout,FlyoutUntilText,until);
    flyoutBaseControls=flyoutLayout.controls;ClampFlyoutWidth();FlyoutRefresh();LayoutFlyout();SetTimer(flyout,1,1000,nullptr);
#ifndef DAC_HARNESS
    ShowWindow(flyout,SW_SHOWNORMAL);SetForegroundWindow(flyout);SetFocus(GetDlgItem(flyout,FlyoutList));
#endif
}
