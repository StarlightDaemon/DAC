// SPDX-License-Identifier: MIT
#pragma once
// Included inside namespace dac after flyout.hpp. All data is the UI-owned
// catalog; neither rendering nor selection performs discovery or policy input.
void ShowInspector(const std::string& id);
constexpr wchar_t kDisplayMap[]=L"DAC-DisplayMap",kMapCanvas[]=L"DAC-DisplayMapCanvas";
enum MapControl {MapCanvas=801,MapList,MapDetail,MapControls,MapSettings,MapInspector,MapStatus};
HWND displayMap{};
HFONT mapFont{};
SettingsLayout mapLayout;
int mapScroll=0;
bool mapRegistered=false;
#ifdef DAC_HARNESS
BOOL mapFontDeleted=TRUE;
#endif
Time mapTopology=0;
std::string mapSelected;
std::vector<std::string> mapIds;
std::vector<std::wstring> mapRows,mapLabels;
std::vector<RECT> mapDesktopRects;

bool MapValidRect(const RECT& r){return r.right>r.left&&r.bottom>r.top;}
bool MapOverlap(const RECT& a,const RECT& b){return MapValidRect(a)&&MapValidRect(b)&&std::max(a.left,b.left)<std::min(a.right,b.right)&&std::max(a.top,b.top)<std::min(a.bottom,b.bottom);}
// Long double arithmetic avoids signed LONG subtraction/multiplication overflow,
// even for the entire signed desktop range. One common scale preserves gaps,
// overlap and portrait aspect ratios; DPI never changes desktop pixel geometry.
std::vector<RECT> MapFit(const std::vector<RECT>& input,const RECT& viewport) {
    std::vector<RECT> result(input.size());
    if(!MapValidRect(viewport))return result;
    long double left=0,top=0,right=0,bottom=0;bool any=false;
    for(auto r:input)if(MapValidRect(r)){if(!any){left=r.left;top=r.top;right=r.right;bottom=r.bottom;any=true;}else{left=std::min(left,static_cast<long double>(r.left));top=std::min(top,static_cast<long double>(r.top));right=std::max(right,static_cast<long double>(r.right));bottom=std::max(bottom,static_cast<long double>(r.bottom));}}
    if(!any)return result;
    auto width=static_cast<long double>(viewport.right)-viewport.left,height=static_cast<long double>(viewport.bottom)-viewport.top;
    auto scale=std::min(width/(right-left),height/(bottom-top));
    auto x=viewport.left+(width-(right-left)*scale)/2,y=viewport.top+(height-(bottom-top)*scale)/2;
    for(size_t i=0;i<input.size();++i)if(MapValidRect(input[i])){
        auto r=input[i];auto pixel=[](long double v){return static_cast<LONG>(v>=0?v+0.5L:v-0.5L);};
        result[i]={pixel(x+(static_cast<long double>(r.left)-left)*scale),pixel(y+(static_cast<long double>(r.top)-top)*scale),pixel(x+(static_cast<long double>(r.right)-left)*scale),pixel(y+(static_cast<long double>(r.bottom)-top)*scale)};
    }
    return result;
}
bool MapAmbiguous(size_t index,const std::vector<Display>& snapshot) {
    for(size_t i=0;i<snapshot.size();++i)if(i!=index&&(snapshot[i].id==snapshot[index].id||MapOverlap(snapshot[i].rect,snapshot[index].rect)))return true;
    return false;
}
const Display* MapTarget(bool eligible) {
    if(topologyPending||mapTopology!=topologyEpoch)return nullptr;
    for(size_t i=0;i<displays.size();++i)if(displays[i].id==mapSelected){auto& d=displays[i];if(eligible&&(!d.identified||!MapValidRect(d.rect)||MapAmbiguous(i,displays)||!controller.nodes.contains(d.id)||controller.unidentified.contains(d.id)))return nullptr;return &d;}
    return nullptr;
}
void MapSetText(int id,const std::wstring& value){if(ControlText(displayMap,id)!=value)SetDlgItemTextW(displayMap,id,value.c_str());}
void MapRefresh() {
    if(!displayMap)return;
    auto list=GetDlgItem(displayMap,MapList);if(!list)return;
    std::vector<std::string> ids;std::vector<std::wstring> rows,labels;std::vector<RECT> rects;
    for(size_t i=0;i<displays.size();++i){auto& d=displays[i];auto name=DisplayLabel(d,controller.config,displays);
        std::wstring status=controller.Pref(d.id).enabled?L"Enabled":L"Disabled";
        if(!d.identified||!MapValidRect(d.rect)||!controller.nodes.contains(d.id)||controller.unidentified.contains(d.id))status+=L"; unknown target — controls unavailable";
        if(MapAmbiguous(i,displays))status+=L"; clone or overlapping target — controls unavailable";
        if(topologyPending)status+=L"; topology refreshing";
        auto geometry=L"; desktop pixels ("+std::to_wstring(d.rect.left)+L", "+std::to_wstring(d.rect.top)+L") to ("+std::to_wstring(d.rect.right)+L", "+std::to_wstring(d.rect.bottom)+L")";
        ids.push_back(d.id);labels.push_back(name+L"\n"+status);rows.push_back(name+L"; "+status+geometry);rects.push_back(d.rect);
    }
    if(mapSelected.empty()&&!ids.empty())mapSelected=ids.front();
    if(ids!=mapIds||rows!=mapRows){int top=static_cast<int>(SendMessageW(list,LB_GETTOPINDEX,0,0));std::string topId=top>=0&&top<static_cast<int>(mapIds.size())?mapIds[top]:"";
        SendMessageW(list,WM_SETREDRAW,FALSE,0);SendMessageW(list,LB_RESETCONTENT,0,0);
        for(auto& row:rows)SendMessageW(list,LB_ADDSTRING,0,reinterpret_cast<LPARAM>(row.c_str()));
        int selected=-1;for(size_t i=0;i<ids.size();++i){if(ids[i]==mapSelected)selected=static_cast<int>(i);if(ids[i]==topId)top=static_cast<int>(i);}
        SendMessageW(list,LB_SETCURSEL,selected,0);if(!ids.empty())SendMessageW(list,LB_SETTOPINDEX,std::clamp(top,0,static_cast<int>(ids.size())-1),0);
        SendMessageW(list,WM_SETREDRAW,TRUE,0);InvalidateRect(list,nullptr,TRUE);
    }
    mapIds=std::move(ids);mapRows=std::move(rows);mapLabels=std::move(labels);mapDesktopRects=std::move(rects);mapTopology=topologyEpoch;
    HDC dc=GetDC(list);if(dc){auto old=mapFont?SelectObject(dc,mapFont):nullptr;int extent=0;for(auto& row:mapRows){SIZE size{};if(GetTextExtentPoint32W(dc,row.c_str(),static_cast<int>(row.size()),&size))extent=std::max(extent,static_cast<int>(size.cx));}if(old)SelectObject(dc,old);ReleaseDC(list,dc);SendMessageW(list,LB_SETHORIZONTALEXTENT,extent+MulDiv(12,mapLayout.dpi,96),0);}
    std::wstring detail=mapIds.empty()?L"No connected displays.":L"Selected display disconnected. Select a connected display to continue.";
    for(size_t i=0;i<mapIds.size();++i)if(mapIds[i]==mapSelected){detail=mapRows[i];break;}
    MapSetText(MapDetail,detail);bool target=MapTarget(true)!=nullptr;
    MapSetText(MapStatus,DisplaySnapshotIsStale(GetTickCount64())?L"Connected &displays (arrow keys) — OS snapshot stale / unknown; use Inspector to rescan":L"Connected &displays (arrow keys) — cached OS snapshot current");
    EnableWindow(GetDlgItem(displayMap,MapControls),target);EnableWindow(GetDlgItem(displayMap,MapSettings),target);EnableWindow(GetDlgItem(displayMap,MapInspector),MapTarget(false)!=nullptr);
    InvalidateRect(GetDlgItem(displayMap,MapCanvas),nullptr,TRUE);
}
void MapPaint(HWND w,HDC dc) {
    RECT client{};GetClientRect(w,&client);FillColor(dc,client,theme.colors.surface);
    int pad=MulDiv(12,mapLayout.dpi,96);RECT viewport{pad,pad,std::max<LONG>(pad,client.right-pad),std::max<LONG>(pad,client.bottom-pad)};
    auto fitted=MapFit(mapDesktopRects,viewport);int saved=SaveDC(dc);SetBkMode(dc,TRANSPARENT);if(mapFont)SelectObject(dc,mapFont);
    for(size_t i=0;i<fitted.size();++i){auto r=fitted[i];if(!MapValidRect(r))continue;bool selected=mapIds[i]==mapSelected;
        auto bg=selected?(theme.highContrast?GetSysColor(COLOR_HIGHLIGHT):theme.colors.accent):theme.colors.base;
        auto fg=selected?(theme.highContrast?GetSysColor(COLOR_HIGHLIGHTTEXT):theme.colors.onAccent):theme.colors.text;
        FillColor(dc,r,bg);auto brush=CreateSolidBrush(fg);FrameRect(dc,&r,brush);DeleteObject(brush);SetTextColor(dc,fg);
        InflateRect(&r,-MulDiv(5,mapLayout.dpi,96),-MulDiv(5,mapLayout.dpi,96));
        auto text=(selected?std::wstring(L"[Selected]\n"):std::wstring{})+mapLabels[i];
        if(MapValidRect(r))DrawTextW(dc,text.c_str(),-1,&r,DT_WORDBREAK|DT_NOPREFIX|DT_END_ELLIPSIS);
    }
    if(mapIds.empty()){SetTextColor(dc,theme.colors.text);DrawTextW(dc,L"No connected displays",-1,&client,DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX);}
    RestoreDC(dc,saved);
}
LRESULT CALLBACK MapCanvasProc(HWND w,UINT msg,WPARAM wp,LPARAM lp) {
    if(msg==WM_PAINT){PAINTSTRUCT ps{};auto dc=BeginPaint(w,&ps);MapPaint(w,dc);EndPaint(w,&ps);return 0;}
    if(msg==WM_PRINTCLIENT){MapPaint(w,reinterpret_cast<HDC>(wp));return 0;}
    if(msg==WM_ERASEBKGND)return 1;
    if(msg==WM_LBUTTONDOWN){RECT r{};GetClientRect(w,&r);int pad=MulDiv(12,mapLayout.dpi,96);r={pad,pad,std::max<LONG>(pad,r.right-pad),std::max<LONG>(pad,r.bottom-pad)};auto fitted=MapFit(mapDesktopRects,r);POINT p{static_cast<short>(LOWORD(lp)),static_cast<short>(HIWORD(lp))};
        for(size_t i=fitted.size();i>0;--i)if(PtInRect(&fitted[i-1],p)){mapSelected=mapIds[i-1];MapRefresh();SendDlgItemMessageW(displayMap,MapList,LB_SETCURSEL,i-1,0);SetFocus(GetDlgItem(displayMap,MapList));break;}return 0;}
    return DefWindowProcW(w,msg,wp,lp);
}
void LayoutDisplayMap(){if(!displayMap)return;RECT r{};GetClientRect(displayMap,&r);int width=std::max(1,MulDiv(r.right,96,mapLayout.dpi)-32);for(auto& c:mapLayout.controls){c.x=16;c.width=width;}LayoutSettings(displayMap,mapLayout,710,mapScroll);}
void RefreshDisplayMapTheme(){if(!displayMap)return;if(auto font=SettingsFont(mapLayout.dpi)){for(auto& c:mapLayout.controls)SendMessageW(c.window,WM_SETFONT,reinterpret_cast<WPARAM>(font),FALSE);if(mapFont)DeleteObject(mapFont);mapFont=font;}ThemeChrome(displayMap);RedrawWindow(displayMap,nullptr,nullptr,RDW_INVALIDATE|RDW_ERASE|RDW_ALLCHILDREN);}
void MapReveal(HWND child){RECT client{},r{};GetClientRect(displayMap,&client);GetWindowRect(child,&r);MapWindowPoints(nullptr,displayMap,reinterpret_cast<POINT*>(&r),2);int delta=r.top<0?r.top:r.bottom>client.bottom?r.bottom-client.bottom:0;if(delta){mapScroll+=delta;LayoutDisplayMap();}}
LRESULT CALLBACK DisplayMapProc(HWND w,UINT msg,WPARAM wp,LPARAM lp) {
    LRESULT result{};if(ThemeMessage(w,msg,wp,lp,result))return result;
    if(msg==WM_SIZE){LayoutDisplayMap();return 0;}
    if(ScrollSettings(w,msg,wp,lp,710,mapScroll,mapLayout))return 0;
    if(msg==WM_TIMER){if(wp==1)MapRefresh();return 0;}
    if(msg==WM_COMMAND){int id=LOWORD(wp),event=HIWORD(wp);
        if(event==BN_SETFOCUS||event==EN_SETFOCUS||(id==MapList&&event==LBN_SETFOCUS)){MapReveal(reinterpret_cast<HWND>(lp));return 0;}
        if(id==IDCANCEL&&event==BN_CLICKED){DestroyWindow(w);return 0;}
        if(id==MapList&&event==LBN_SELCHANGE){int index=static_cast<int>(SendDlgItemMessageW(w,MapList,LB_GETCURSEL,0,0));if(index>=0&&index<static_cast<int>(mapIds.size()))mapSelected=mapIds[index];MapRefresh();return 0;}
        if(event==BN_CLICKED){bool eligible=id==MapControls||id==MapSettings;if(!MapTarget(eligible)){MapSetText(MapDetail,L"The display snapshot changed or the target is unavailable. Review the refreshed map and try again.");return 0;}
            if(id==MapControls){flyoutSelected=mapSelected;ShowFlyout();}
            else if(id==MapSettings){if(!ShowDisplaySettings(mapSelected))MapSetText(MapDetail,L"Finish the open settings dialog or correct its draft before selecting this display.");}
            else if(id==MapInspector)ShowInspector(mapSelected);
        }return 0;}
    if(msg==WM_DPICHANGED){SettingsDpiChanged(w,wp,lp,mapLayout,mapFont,710,mapScroll);LayoutDisplayMap();return 0;}
    if(msg==WM_CLOSE){DestroyWindow(w);return 0;}
    if(msg==WM_DESTROY){KillTimer(w,1);{std::lock_guard lock(registryLock);ownedWindows.erase(w);}displayMap=nullptr;return 0;}
    if(msg==WM_NCDESTROY){mapLayout.controls.clear();mapIds.clear();mapRows.clear();mapLabels.clear();mapDesktopRects.clear();if(mapFont){
#ifdef DAC_HARNESS
        mapFontDeleted=DeleteObject(mapFont);
#else
        DeleteObject(mapFont);
#endif
        mapFont=nullptr;}}
    return DefWindowProcW(w,msg,wp,lp);
}
void CloseDisplayMap(){if(displayMap)DestroyWindow(displayMap);}
void ShowDisplayMap(){
    if(displayMap){MapRefresh();
#ifndef DAC_HARNESS
        ShowWindow(displayMap,SW_SHOWNORMAL);SetForegroundWindow(displayMap);
#endif
        return;}
    if(!mapRegistered){WNDCLASSW wc{};wc.hInstance=GetModuleHandleW(nullptr);wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);wc.lpfnWndProc=MapCanvasProc;wc.lpszClassName=kMapCanvas;if(!RegisterClassW(&wc)&&GetLastError()!=ERROR_CLASS_ALREADY_EXISTS)return;wc.lpfnWndProc=DisplayMapProc;wc.lpszClassName=kDisplayMap;if(!RegisterClassW(&wc)&&GetLastError()!=ERROR_CLASS_ALREADY_EXISTS)return;mapRegistered=true;}
    mapLayout={};mapScroll=0;displayMap=CreateWindowExW(WS_EX_TOOLWINDOW|WS_EX_CONTROLPARENT,kDisplayMap,L"DAC — Display map",WS_CAPTION|WS_SYSMENU|WS_THICKFRAME|WS_VSCROLL|WS_CLIPCHILDREN,CW_USEDEFAULT,CW_USEDEFAULT,780,760,ui,nullptr,GetModuleHandleW(nullptr),nullptr);if(!displayMap)return;
    {std::lock_guard lock(registryLock);ownedWindows.insert(displayMap);}FitSettings(displayMap,780,760);mapLayout.dpi=GetDpiForWindow(displayMap);RefreshDisplayMapTheme();
    auto control=[&](const wchar_t* cls,const wchar_t* title,DWORD style,int y,int height,int id){auto child=CreateWindowExW(0,cls,title,WS_CHILD|WS_VISIBLE|style,0,0,1,1,displayMap,reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),GetModuleHandleW(nullptr),nullptr);mapLayout.controls.push_back({child,16,y,730,height});ThemeControl(child,cls);SendMessageW(child,WM_SETFONT,reinterpret_cast<WPARAM>(mapFont?mapFont:GetStockObject(DEFAULT_GUI_FONT)),TRUE);return child;};
    control(L"STATIC",L"Desktop pixels. Legend: [Selected] = current target; Enabled / Disabled = policy; Unknown / clone = unavailable. Use rectangles or the keyboard display list.",SS_NOPREFIX,12,70,0);
    control(kMapCanvas,L"Display layout; use the connected displays list for keyboard selection",0,86,222,MapCanvas);
    control(L"STATIC",L"Connected &displays (arrow keys select)",0,312,36,MapStatus);
    control(L"LISTBOX",L"",WS_BORDER|WS_TABSTOP|WS_VSCROLL|WS_HSCROLL|LBS_NOTIFY|LBS_NOINTEGRALHEIGHT,352,104,MapList);
    control(L"STATIC",L"Selected display details",SS_NOPREFIX,466,22,0);
    control(L"EDIT",L"",WS_TABSTOP|WS_BORDER|WS_VSCROLL|ES_MULTILINE|ES_READONLY,492,80,MapDetail);
    control(L"BUTTON",L"Open display &controls…",BS_PUSHBUTTON|BS_NOTIFY|WS_TABSTOP,582,28,MapControls);
    control(L"BUTTON",L"Display &settings…",BS_PUSHBUTTON|BS_NOTIFY|WS_TABSTOP,614,28,MapSettings);
    control(L"BUTTON",L"&Inspect selected display…",BS_PUSHBUTTON|BS_NOTIFY|WS_TABSTOP,646,28,MapInspector);
    control(L"BUTTON",L"&Close",BS_PUSHBUTTON|BS_NOTIFY|WS_TABSTOP,678,28,IDCANCEL);
    MapRefresh();LayoutDisplayMap();SetTimer(displayMap,1,1000,nullptr);
#ifndef DAC_HARNESS
    ShowWindow(displayMap,SW_SHOWNORMAL);SetForegroundWindow(displayMap);SetFocus(GetDlgItem(displayMap,MapList));
#endif
}
