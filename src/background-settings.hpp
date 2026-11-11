// SPDX-License-Identifier: MIT
#pragma once
// Nested settings draft: only Apply changes workspaceRules; main Save persists.
constexpr wchar_t kBackgroundEditor[]=L"DAC-BackgroundRule";
HWND backgroundEditor{};HFONT backgroundEditorFont{};SettingsLayout backgroundEditorLayout;int backgroundEditorScroll=0,backgroundEditIndex=-1;
bool backgroundEditorRegistered=false;AppRule backgroundEditRule;std::string backgroundEditIdentity;
std::vector<std::string> backgroundTargetIds;
void RefreshBackgroundEditorTheme(){if(!backgroundEditor)return;if(auto font=SettingsFont(backgroundEditorLayout.dpi)){for(auto& c:backgroundEditorLayout.controls)SendMessageW(c.window,WM_SETFONT,reinterpret_cast<WPARAM>(font),FALSE);if(backgroundEditorFont)DeleteObject(backgroundEditorFont);backgroundEditorFont=font;}ThemeChrome(backgroundEditor);RedrawWindow(backgroundEditor,nullptr,nullptr,RDW_INVALIDATE|RDW_ERASE|RDW_ALLCHILDREN);}
void BackgroundEditorReveal(HWND child){RECT client{},r{};GetClientRect(backgroundEditor,&client);GetWindowRect(child,&r);MapWindowPoints(nullptr,backgroundEditor,reinterpret_cast<POINT*>(&r),2);int delta=r.top<0?r.top:r.bottom>client.bottom?r.bottom-client.bottom:0;if(delta){backgroundEditorScroll+=delta;LayoutSettings(backgroundEditor,backgroundEditorLayout,600,backgroundEditorScroll);}}
std::string WorkspaceRuleIdentity(){Config c;c.rules=workspaceRules;return Serialize(c);}
std::wstring SelectApplication(HWND owner) {
    std::vector<wchar_t> file(32768);OPENFILENAMEW d{};d.lStructSize=sizeof(d);d.hwndOwner=owner;d.lpstrFile=file.data();d.nMaxFile=static_cast<DWORD>(file.size());
    d.lpstrFilter=L"Application executable\0*.exe\0All files\0*.*\0";d.lpstrTitle=L"Choose application to observe (DAC does not launch it)";d.Flags=OFN_NOCHANGEDIR|OFN_PATHMUSTEXIST|OFN_FILEMUSTEXIST;
    return GetOpenFileNameW(&d)?file.data():L"";
}
bool ApplyBackgroundRuleDraft() {
    if(!workspace||backgroundEditIdentity!=WorkspaceRuleIdentity())return false;
    auto rule=backgroundEditRule;rule.kind=2;rule.executable=Utf8(ControlText(backgroundEditor,1001));rule.enabled=IsDlgButtonChecked(backgroundEditor,1002)==BST_CHECKED;
    rule.nameOnly=IsDlgButtonChecked(backgroundEditor,1003)==BST_CHECKED;rule.global=IsDlgButtonChecked(backgroundEditor,1004)==BST_CHECKED;rule.targets.clear();
    if(!rule.global)for(size_t i=0;i<backgroundTargetIds.size();++i)if(SendDlgItemMessageW(backgroundEditor,1005,LB_GETSEL,i,0)>0)rule.targets.insert(backgroundTargetIds[i]);
    Config check;check.rules=workspaceRules;if(backgroundEditIndex<0)check.rules.push_back(rule);else {if(backgroundEditIndex>=static_cast<int>(check.rules.size()))return false;check.rules[backgroundEditIndex]=rule;}
    Config parsed;std::string error;if(!Parse(Serialize(check),parsed,error))return false;workspaceRules=parsed.rules;RefreshRuleList();return true;
}
LRESULT CALLBACK BackgroundEditorProc(HWND w,UINT msg,WPARAM wp,LPARAM lp) {
    LRESULT result{};if(ThemeMessage(w,msg,wp,lp,result))return result;
    if(ScrollSettings(w,msg,wp,lp,600,backgroundEditorScroll,backgroundEditorLayout))return 0;
    if(msg==WM_DPICHANGED){SettingsDpiChanged(w,wp,lp,backgroundEditorLayout,backgroundEditorFont,600,backgroundEditorScroll);return 0;}
    if(msg==WM_COMMAND){int id=LOWORD(wp);
        int event=HIWORD(wp);if(event==EN_SETFOCUS||event==BN_SETFOCUS||(id==1005&&event==LBN_SETFOCUS)){BackgroundEditorReveal(reinterpret_cast<HWND>(lp));return 0;}
        if(event!=BN_CLICKED)return 0;
        if(id==1006){auto path=SelectApplication(w);if(!path.empty()){SetDlgItemTextW(w,1001,path.c_str());CheckDlgButton(w,1003,BST_UNCHECKED);}}
        else if(id==1004)EnableWindow(GetDlgItem(w,1005),IsDlgButtonChecked(w,1004)!=BST_CHECKED);
        else if(id==IDOK){if(ApplyBackgroundRuleDraft())DestroyWindow(w);else SetDlgItemTextW(w,1007,L"Choose an executable and explicit global scope or at least one stable display. Maximum 32 rules. If the parent draft changed, Cancel and reopen.");}
        else if(id==IDCANCEL)DestroyWindow(w);return 0;
    }
    if(msg==WM_CLOSE){DestroyWindow(w);return 0;}
    if(msg==WM_DESTROY){backgroundEditor=nullptr;if(workspace){EnableWindow(workspace,TRUE);SetForegroundWindow(workspace);}return 0;}
    if(msg==WM_NCDESTROY){backgroundEditorLayout.controls.clear();backgroundTargetIds.clear();if(backgroundEditorFont){DeleteObject(backgroundEditorFont);backgroundEditorFont=nullptr;}}
    return DefWindowProcW(w,msg,wp,lp);
}
void ShowBackgroundRuleEditor(int index=-1) {
    if(!workspace)return;if(backgroundEditor){SetForegroundWindow(backgroundEditor);return;}
    if(index>=0&&(index>=static_cast<int>(workspaceRules.size())||workspaceRules[index].kind!=2)){Notice(L"Select a background rule to edit.");return;}
    backgroundEditIndex=index;backgroundEditIdentity=WorkspaceRuleIdentity();backgroundEditRule=index<0?AppRule{}:workspaceRules[index];backgroundEditRule.kind=2;
    if(!backgroundEditorRegistered){WNDCLASSW wc{};wc.hInstance=GetModuleHandleW(nullptr);wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);wc.lpfnWndProc=BackgroundEditorProc;wc.lpszClassName=kBackgroundEditor;if(!RegisterClassW(&wc))return;backgroundEditorRegistered=true;}
    backgroundEditorLayout={};backgroundEditorScroll=0;backgroundEditor=CreateWindowExW(WS_EX_DLGMODALFRAME|WS_EX_CONTROLPARENT,kBackgroundEditor,L"Background running-application rule — unsaved draft",WS_CAPTION|WS_SYSMENU|WS_VSCROLL,CW_USEDEFAULT,CW_USEDEFAULT,740,680,workspace,nullptr,GetModuleHandleW(nullptr),nullptr);if(!backgroundEditor)return;
    FitSettings(backgroundEditor,740,680);backgroundEditorLayout.dpi=GetDpiForWindow(backgroundEditor);RefreshBackgroundEditorTheme();
    auto control=[&](const wchar_t* cls,const wchar_t* text,DWORD style,int y,int height,int id){if(wcscmp(cls,L"BUTTON")==0)style|=BS_NOTIFY;auto child=CreateWindowExW(0,cls,text,WS_CHILD|WS_VISIBLE|style,0,0,1,1,backgroundEditor,reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),GetModuleHandleW(nullptr),nullptr);backgroundEditorLayout.controls.push_back({child,16,y,680,height});ThemeControl(child,cls);SendMessageW(child,WM_SETFONT,reinterpret_cast<WPARAM>(backgroundEditorFont),TRUE);};
    control(L"STATIC",L"Inhibit DAC automatic protection while any matching application instance runs in your user/session. This does not prevent Windows sleep or grant hardware control.",SS_NOPREFIX,12,64,0);
    control(L"STATIC",L"Application &executable (full path preferred)",0,82,24,0);control(L"EDIT",Wide(backgroundEditRule.executable).c_str(),WS_BORDER|WS_TABSTOP|ES_AUTOHSCROLL,110,28,1001);
    SendDlgItemMessageW(backgroundEditor,1001,EM_SETLIMITTEXT,4096,0);
    control(L"BUTTON",L"&Choose executable…",BS_PUSHBUTTON|WS_TABSTOP,146,32,1006);
    control(L"BUTTON",L"Rule &enabled",BS_AUTOCHECKBOX|WS_TABSTOP,188,28,1002);
    control(L"BUTTON",L"Match filename &only — includes other applications with the same filename",BS_AUTOCHECKBOX|WS_TABSTOP,224,36,1003);
    control(L"BUTTON",L"Explicit &global scope — all eligible displays",BS_AUTOCHECKBOX|WS_TABSTOP,268,28,1004);
    control(L"STATIC",L"Or select &stable display targets (Space toggles, arrows navigate)",0,306,24,0);
    control(L"LISTBOX",L"",WS_BORDER|WS_TABSTOP|WS_VSCROLL|LBS_NOTIFY|LBS_MULTIPLESEL|LBS_NOINTEGRALHEIGHT,338,130,1005);
    backgroundTargetIds.clear();for(auto& d:displays)if(d.identified){backgroundTargetIds.push_back(d.id);auto name=DisplayLabel(d,draft,displays);SendDlgItemMessageW(backgroundEditor,1005,LB_ADDSTRING,0,reinterpret_cast<LPARAM>(name.c_str()));}
    for(auto& id:backgroundEditRule.targets)if(std::find(backgroundTargetIds.begin(),backgroundTargetIds.end(),id)==backgroundTargetIds.end()){backgroundTargetIds.push_back(id);auto label=L"Disconnected saved target — "+Wide(id);SendDlgItemMessageW(backgroundEditor,1005,LB_ADDSTRING,0,reinterpret_cast<LPARAM>(label.c_str()));}
    for(size_t i=0;i<backgroundTargetIds.size();++i)if(backgroundEditRule.targets.contains(backgroundTargetIds[i]))SendDlgItemMessageW(backgroundEditor,1005,LB_SETSEL,TRUE,i);
    for(auto pair:{std::pair{1002,backgroundEditRule.enabled},std::pair{1003,backgroundEditRule.nameOnly},std::pair{1004,backgroundEditRule.global}})CheckDlgButton(backgroundEditor,pair.first,pair.second?BST_CHECKED:BST_UNCHECKED);
    EnableWindow(GetDlgItem(backgroundEditor,1005),!backgroundEditRule.global);
    control(L"STATIC",L"Apply adds or replaces this rule in the parent draft. Apply rules there, then Save in Advanced settings. Cancel leaves the rule unchanged.",SS_NOPREFIX,478,48,1007);
    control(L"BUTTON",L"&Apply rule to draft",BS_PUSHBUTTON|WS_TABSTOP,534,28,IDOK);control(L"BUTTON",L"&Cancel",BS_PUSHBUTTON|WS_TABSTOP,570,28,IDCANCEL);
    LayoutSettings(backgroundEditor,backgroundEditorLayout,600,backgroundEditorScroll);EnableWindow(workspace,FALSE);
#ifndef DAC_HARNESS
    ShowWindow(backgroundEditor,SW_SHOWNORMAL);SetForegroundWindow(backgroundEditor);SetFocus(GetDlgItem(backgroundEditor,1001));
#endif
}
