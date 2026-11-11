// SPDX-License-Identifier: MIT
// Private native UI fixture. Include after features-platform.hpp; host must be initialized.
#pragma once
void CaptureHidden(HWND window,const wchar_t* suffix);

void FeatureFlyoutSelect(const std::string& id) {
    auto found=std::find(flyoutIds.begin(),flyoutIds.end(),id);Check(found!=flyoutIds.end(),"flyout selected fixture target exists");
    auto index=static_cast<int>(found-flyoutIds.begin());SendDlgItemMessageW(flyout,FlyoutList,LB_SETCURSEL,index,0);
    SendMessageW(flyout,WM_COMMAND,MAKEWPARAM(FlyoutList,LBN_SELCHANGE),reinterpret_cast<LPARAM>(GetDlgItem(flyout,FlyoutList)));
}
LRESULT FeatureFlyoutRetention() {
    SetupSessions();topologyPending=false;flyoutSelected.clear();ShowFlyout();
    Check(flyout&&flyoutFont&&flyoutLayout.controls.size()>20&&!IsWindowVisible(flyout),"flyout native controls created only in hidden fixture");
    {std::lock_guard lock(registryLock);Check(ownedWindows.contains(flyout),"flyout registered as owned foreground window");}
    Check(SendDlgItemMessageW(flyout,FlyoutList,LB_GETCOUNT,0,0)==3,"flyout has row for every connected display");
    auto window=flyout,edit=GetDlgItem(flyout,FlyoutMinutes),until=GetDlgItem(flyout,FlyoutUntilText),list=GetDlgItem(flyout,FlyoutList);
    FeatureFlyoutSelect("B");SetWindowTextW(edit,L"137");SetWindowTextW(until,L"2030-01-01 12:34");SendMessageW(edit,EM_SETSEL,1,2);
    Check(edit&&IsWindowEnabled(edit),"flyout focus target is an enabled native Edit");SetFocus(edit);auto focus=GetFocus();Check(focus==edit,"flyout retention exercise actually focuses intended Edit");
    auto selected=controller.nodes.at("B"),peer=controller.nodes.at("A");
    SendMessageW(flyout,WM_TIMER,1,0);std::reverse(displays.begin(),displays.end());SendMessageW(flyout,WM_TIMER,1,0);
    DWORD first=0,last=0;SendMessageW(edit,EM_GETSEL,reinterpret_cast<WPARAM>(&first),reinterpret_cast<LPARAM>(&last));
    Check(flyout==window&&GetDlgItem(flyout,FlyoutMinutes)==edit&&GetDlgItem(flyout,FlyoutUntilText)==until&&GetFocus()==focus,"flyout countdown/reorder retains controls and focus");
    Check(ControlText(flyout,FlyoutMinutes)==L"137"&&ControlText(flyout,FlyoutUntilText)==L"2030-01-01 12:34"&&first==1&&last==2,"flyout refresh retains custom duration/expiry drafts and edit selection");
    int index=static_cast<int>(SendMessageW(list,LB_GETCURSEL,0,0));Check(index>=0&&flyoutIds[index]=="B"&&flyoutSelected=="B","flyout selection remains bound to stable identity after reorder");
    Check(FeaturePlatformNodeSame(peer,controller.nodes.at("A"))&&FeaturePlatformNodeSame(selected,controller.nodes.at("B")),"flyout refresh does not change controller generations or peers");
    auto oldEpoch=flyoutTopology;++topologyEpoch;
    SendMessageW(flyout,WM_COMMAND,MAKEWPARAM(Flyout5,BN_CLICKED),reinterpret_cast<LPARAM>(GetDlgItem(flyout,Flyout5)));
    Check(oldEpoch!=topologyEpoch&&!controller.leases.contains("B")&&!ControlText(flyout,FlyoutError).empty(),"flyout command rejects stale captured topology before timer refresh");
    topologyPending=true;FlyoutRefresh();Check(!IsWindowEnabled(GetDlgItem(flyout,FlyoutStart))&&!IsWindowEnabled(GetDlgItem(flyout,Flyout5)),"flyout pending topology disables target actions");topologyPending=false;
    displays.erase(std::remove_if(displays.begin(),displays.end(),[](const Display& display){return display.id=="B";}),displays.end());controller.nodes.erase("B");++topologyEpoch;FlyoutRefresh();
    Check(flyoutSelected=="B"&&SendMessageW(list,LB_GETCURSEL,0,0)==LB_ERR&&!IsWindowEnabled(GetDlgItem(flyout,FlyoutStart))&&!IsWindowEnabled(GetDlgItem(flyout,FlyoutSettings)),"flyout disconnected selection stays explicit and cannot redirect commands");
    SendMessageW(flyout,WM_COMMAND,MAKEWPARAM(Flyout5,BN_CLICKED),reinterpret_cast<LPARAM>(GetDlgItem(flyout,Flyout5)));
    Check(controller.leases.empty()&&FeaturePlatformNodeSame(peer,controller.nodes.at("A")),"flyout removed row cannot grant a lease to a peer");
    SetupSessions();++topologyEpoch;FlyoutRefresh();Check(flyoutSelected=="B"&&SendMessageW(list,LB_GETCURSEL,0,0)==1,"flyout reconnect restores the same stable target");
    auto before=controller.nodes.at("A");SendMessageW(flyout,WM_COMMAND,MAKEWPARAM(Flyout5,BN_CLICKED),reinterpret_cast<LPARAM>(GetDlgItem(flyout,Flyout5)));
    Check(controller.leases.contains("B")&&FeaturePlatformNodeSame(before,controller.nodes.at("A"))&&IsWindowEnabled(GetDlgItem(flyout,FlyoutCancel)),"flyout five-minute preset grants only selected display lease");
    auto deadline=controller.leases.at("B").deadline;FlyoutRefresh();
    auto length=static_cast<int>(SendMessageW(list,LB_GETTEXTLEN,1,0));std::wstring native(static_cast<size_t>(length)+1,L'\0');SendMessageW(list,LB_GETTEXT,1,reinterpret_cast<LPARAM>(native.data()));native.resize(length);
    Check(native.find(L"Injected B")!=std::wstring::npos&&native.find(L"\r\n")!=std::wstring::npos&&native.find(L"keep awake")!=std::wstring::npos&&ControlText(flyout,FlyoutDetail).find(L"keep awake")!=std::wstring::npos,"flyout native accessibility strings include complete name/state/reason/countdown");
    controller.Manual("B",GetTickCount64(),true,-1);FlyoutRefresh();
    Check(IsWindowEnabled(GetDlgItem(flyout,FlyoutStop))&&!IsWindowEnabled(GetDlgItem(flyout,Flyout5))&&IsWindowEnabled(GetDlgItem(flyout,FlyoutCancel)),"flyout manual presentation can stop or cancel existing lease but cannot create one");
    auto manual=controller.nodes.at("B");SendMessageW(flyout,WM_COMMAND,MAKEWPARAM(FlyoutCustom,BN_CLICKED),reinterpret_cast<LPARAM>(GetDlgItem(flyout,FlyoutCustom)));
    Check(controller.leases.at("B").deadline==deadline&&FeaturePlatformNodeSame(manual,controller.nodes.at("B")),"flyout forged disabled lease command cannot replace manual authority");
    controller.nodes["B"]={GetTickCount64(),++controller.serial,State::Desktop,false};controller.nodes["@span"]={GetTickCount64(),++controller.serial,State::Black,true};FlyoutRefresh();
    Check(!IsWindowEnabled(GetDlgItem(flyout,FlyoutStart))&&!IsWindowEnabled(GetDlgItem(flyout,FlyoutStop))&&!IsWindowEnabled(GetDlgItem(flyout,Flyout5))&&!IsWindowEnabled(GetDlgItem(flyout,FlyoutCancel)),"flyout individual controls disabled throughout desktop span");
    SendMessageW(flyout,WM_COMMAND,MAKEWPARAM(FlyoutStop,BN_CLICKED),reinterpret_cast<LPARAM>(GetDlgItem(flyout,FlyoutStop)));Check(controller.Spanning(),"flyout individual stop never implicitly stops desktop span");
    controller.nodes.erase("@span");controller.leases.clear();FlyoutRefresh();return 1;
}
LRESULT FeatureFlyoutDpiThemeAndSettings() {
    auto window=flyout,edit=GetDlgItem(flyout,FlyoutMinutes);SetWindowTextW(edit,L"137");SendMessageW(edit,EM_SETSEL,1,2);
    wchar_t captureFlag[2]{};bool capture=GetEnvironmentVariableW(L"DAC_FLYOUT_SNAPSHOTS",captureFlag,2)==1&&captureFlag[0]==L'1',captured96=false;
    for(int dpi:{96,144,192,96}) {
        auto oldFont=flyoutFont;RECT suggestion{0,0,MulDiv(kFlyoutWidth,dpi,96),MulDiv(810,dpi,96)};
        SendMessageW(flyout,WM_DPICHANGED,MAKEWPARAM(dpi,dpi),reinterpret_cast<LPARAM>(&suggestion));
        Check(flyout==window&&GetDlgItem(flyout,FlyoutMinutes)==edit&&flyoutLayout.dpi==dpi&&ControlText(flyout,FlyoutMinutes)==L"137","flyout DPI transition retains control handles and drafts");
        Check(flyoutFont&&flyoutFont!=oldFont,"flyout DPI transition installs a replacement font");
        RECT available{};GetClientRect(flyout,&available);for(auto& control:flyoutLayout.controls){RECT bounds{};GetWindowRect(control.window,&bounds);MapWindowPoints(nullptr,flyout,reinterpret_cast<POINT*>(&bounds),2);Check(bounds.left>=0&&bounds.right<=available.right,"flyout DPI layout keeps every control inside available client width");}
        for(auto& control:flyoutLayout.controls){RECT bounds{};GetWindowRect(control.window,&bounds);MapWindowPoints(nullptr,flyout,reinterpret_cast<POINT*>(&bounds),2);Check(bounds.left==MulDiv(control.x,dpi,96)&&bounds.top==MulDiv(control.y,dpi,96)-flyoutScroll&&bounds.right-bounds.left==MulDiv(control.width,dpi,96),"flyout controls reflow from logical DPI bounds without drift");}
        Check(SendDlgItemMessageW(flyout,FlyoutList,LB_GETITEMHEIGHT,0,0)>=MulDiv(74,dpi,96),"flyout list rows scale with DPI and system font");
        if(capture&&(dpi!=96||!captured96)){RECT client{},outer{};GetClientRect(flyout,&client);GetWindowRect(flyout,&outer);SetWindowPos(flyout,nullptr,0,0,outer.right-outer.left,MulDiv(kFlyoutHeight,dpi,96)+outer.bottom-outer.top-client.bottom,SWP_NOZORDER|SWP_NOACTIVATE);flyoutScroll=0;LayoutSettings(flyout,flyoutLayout,kFlyoutHeight,flyoutScroll);CaptureHidden(flyout,dpi==96?L".flyout-100.png":dpi==144?L".flyout-150.png":L".flyout-200.png");if(dpi==96)captured96=true;}
    }
    for(int mode:{0,1,2}){testTheme=mode;RefreshSettingsTheme();Check(theme.highContrast==(mode==2)&&flyout==window&&ControlText(flyout,FlyoutMinutes)==L"137","flyout light/dark/high contrast refresh retains drafts");
        HDC dc=GetDC(edit);auto brush=SendMessageW(flyout,WM_CTLCOLOREDIT,reinterpret_cast<WPARAM>(dc),reinterpret_cast<LPARAM>(edit));if(mode!=2)Check(brush==reinterpret_cast<LRESULT>(theme.surface)&&GetTextColor(dc)==theme.colors.text,"flyout native edit uses Fujin theme roles");ReleaseDC(edit,dc);
        auto list=GetDlgItem(flyout,FlyoutList);HDC source=GetDC(list),memory=CreateCompatibleDC(source);auto bitmap=CreateCompatibleBitmap(source,400,FlyoutRowHeight());auto previous=SelectObject(memory,bitmap);DRAWITEMSTRUCT draw{};draw.CtlType=ODT_LISTBOX;draw.CtlID=FlyoutList;draw.itemID=0;draw.itemState=ODS_SELECTED;draw.hwndItem=list;draw.hDC=memory;draw.rcItem={0,0,400,FlyoutRowHeight()};
        Check(FlyoutDrawItem(draw)&&GetPixel(memory,0,0)==(mode==2?GetSysColor(COLOR_HIGHLIGHT):theme.colors.accent),"flyout offscreen selected row paints theme or system high contrast colors");SelectObject(memory,previous);DeleteObject(bitmap);DeleteDC(memory);ReleaseDC(list,source);
    }
    testTheme=-1;RefreshSettingsTheme();SetWindowPos(flyout,nullptr,0,0,kFlyoutWidth,300,SWP_NOZORDER|SWP_NOACTIVATE);SendMessageW(flyout,WM_COMMAND,MAKEWPARAM(FlyoutCustom,BN_SETFOCUS),reinterpret_cast<LPARAM>(GetDlgItem(flyout,FlyoutCustom)));
    RECT client{},button{};GetClientRect(flyout,&client);GetWindowRect(GetDlgItem(flyout,FlyoutCustom),&button);MapWindowPoints(nullptr,flyout,reinterpret_cast<POINT*>(&button),2);Check(button.top>=0&&button.bottom<=client.bottom&&flyoutScroll>0,"flyout keyboard focus reveals controls below a short viewport");
    auto saved=Serialize(controller.config);Check(ShowDisplaySettings("A")&&editorDisplays[selection].id=="A"&&!IsWindowVisible(editor),"flyout Settings opens corresponding stable display in hidden fixture");
    SetDlgItemTextW(editor,169,L"Unsaved A & desk");SetDlgItemTextW(editor,101,L"777");
    Check(ShowDisplaySettings("B")&&editorDisplays[selection].id=="B"&&draft.nicknames.at("A")=="Unsaved A & desk"&&draft.timeout==777&&Serialize(controller.config)==saved,"flyout Settings target change preserves existing nickname/global draft");
    SetDlgItemTextW(editor,169,L"Invalid \x202e");Check(!ShowDisplaySettings("A")&&editorDisplays[selection].id=="B"&&ControlText(editor,169)==L"Invalid \x202e","flyout Settings retains invalid draft and selection until corrected");
    SetDlgItemTextW(editor,169,L"Unsaved B");DestroyWindow(editor);return 1;
}
LRESULT FeatureFlyoutManyAndLifecycle() {
    displays.clear();std::vector<std::string> ids;controller=Controller{};controller.config.media=false;
    for(int i=0;i<64;++i){auto id="fixture-"+std::to_string(i);displays.push_back({id,"",L"Fixture display "+std::to_wstring(i+1),nullptr,{i*20,0,i*20+20,100},true});ids.push_back(id);}
    controller.Topology(ids,GetTickCount64());++topologyEpoch;FlyoutRefresh();auto list=GetDlgItem(flyout,FlyoutList);Check(SendMessageW(list,LB_GETCOUNT,0,0)==64,"flyout lists bounded maximum connected display fixture");
    FeatureFlyoutSelect("fixture-63");SendMessageW(list,LB_SETTOPINDEX,60,0);auto top=SendMessageW(list,LB_GETTOPINDEX,0,0);SendMessageW(flyout,WM_TIMER,1,0);
    Check(flyoutSelected=="fixture-63"&&SendMessageW(list,LB_GETTOPINDEX,0,0)==top&&(GetWindowLongPtrW(list,GWL_STYLE)&WS_VSCROLL),"flyout many-display list retains scroll and stable selection on countdown refresh");
    auto window=flyout;BOOL destroyed=DestroyWindow(flyout);
    printf("FLYOUT cleanup: DestroyWindow=%d live=%d global=%p globalFont=%p controls=%zu DeleteObject=%d deleteError=%lu\n",destroyed,IsWindow(window),static_cast<void*>(flyout),static_cast<void*>(flyoutFont),flyoutLayout.controls.size(),flyoutFontDeleteResult,flyoutFontDeleteError);fflush(stdout);
    Check(destroyed!=FALSE,"flyout DestroyWindow succeeds");Check(!flyout,"flyout destruction clears global HWND");Check(!IsWindow(window),"flyout destruction releases native HWND");
    Check(flyoutLayout.controls.empty(),"flyout destruction clears control layout");Check(!flyoutFont,"flyout destruction clears global font");
    // GetObjectType can report cached deleted objects; it is not a lifetime
    // oracle. Check the actual deletion result and repeated resource bounds.
    // https://learn.microsoft.com/en-us/archive/msdn-magazine/2001/march/resource-leaks-detecting-locating-and-repairing-your-leaky-gdi-code
    Check(flyoutFontDeleteResult!=FALSE,"flyout destruction DeleteObject succeeds");
    {std::lock_guard lock(registryLock);Check(!ownedWindows.contains(window),"flyout destruction removes foreground ownership");}
    return 1;
}

// Evidence associates the delayed count rise with native input initialization;
// individual native allocations and ownership were not identified. Settle the
// initial activity, then also observe delayed closed-state work after each wave.
// Production and the ordinary focus/DPI/theme tests keep native input enabled.
struct FlyoutResourceSample {DWORD gdi=0,user=0,handles=0;};
FlyoutResourceSample flyoutResourceSample,flyoutResourcePeak;
HWND flyoutResourceWindow{};
bool FeatureFlyoutResourcesWithin(const FlyoutResourceSample& baseline,const FlyoutResourceSample& sample) {
    return sample.gdi<=baseline.gdi&&sample.user<=baseline.user&&uint64_t(sample.handles)<=uint64_t(baseline.handles)+4;
}
DWORD FeatureFlyoutGuiCount(DWORD kind) {
    SetLastError(ERROR_SUCCESS);auto count=GetGuiResources(GetCurrentProcess(),kind);
    // The initialized host owns its window/icons/theme brushes even with the
    // flyout closed. Zero is invalid here, including API failure returning zero.
    Check(count!=0,"GUI resource query succeeds with initialized host resources");return count;
}
LRESULT FeatureFlyoutResourceSample() {
    Check(!flyout&&!flyoutFont&&flyoutLayout.controls.empty(),"resource sample has no retained flyout or font");
    Check(!editor&&!displayMap&&!inspector&&!workspace&&!options&&!backgroundEditor&&!setupWindow,"flyout lifecycle did not open another DAC surface");
    {std::lock_guard lock(registryLock);Check(ownedWindows.empty(),"flyout lifecycle retains no owned window registrations");}
    Check(GdiFlush()!=FALSE,"closed-state GDI batch flush succeeds");flyoutResourceSample.gdi=FeatureFlyoutGuiCount(GR_GDIOBJECTS);
    flyoutResourceSample.user=FeatureFlyoutGuiCount(GR_USEROBJECTS);
    Check(GetProcessHandleCount(GetCurrentProcess(),&flyoutResourceSample.handles)!=FALSE,"flyout resource handle sample");
    flyoutResourcePeak.gdi=std::max(flyoutResourcePeak.gdi,flyoutResourceSample.gdi);
    flyoutResourcePeak.user=std::max(flyoutResourcePeak.user,flyoutResourceSample.user);
    flyoutResourcePeak.handles=std::max(flyoutResourcePeak.handles,flyoutResourceSample.handles);return 1;
}
LRESULT FeatureFlyoutResourceOpen() {
    Check(!flyout&&!flyoutResourceWindow,"resource cycle starts without an open flyout");ShowFlyout();flyoutResourceWindow=flyout;
    Check(flyout&&flyoutFont&&!IsWindowVisible(flyout),"flyout repeated open remains hidden");
    ShowFlyout();Check(flyout==flyoutResourceWindow&&!IsWindowVisible(flyout),"flyout repeated show reuses existing hidden window");
    auto edit=GetDlgItem(flyout,FlyoutMinutes);Check(edit&&IsWindowEnabled(edit),"resource cycle has an enabled native Edit");
    SetFocus(edit);Check(GetFocus()==edit,"every resource cycle actually focuses intended Edit");return 1;
}
LRESULT FeatureFlyoutResourceClose() {
    auto opened=flyoutResourceWindow;Check(opened&&flyout==opened&&flyoutFont,"resource cycle retains its intended window");
    auto edit=GetDlgItem(opened,FlyoutMinutes);Check(edit&&IsWindowEnabled(edit)&&GetFocus()==edit,"resource cycle retains enabled Edit focus across UI pumping");
    Check(DestroyWindow(opened)&&!IsWindow(opened)&&!flyout&&!flyoutFont&&flyoutFontDeleteResult,"flyout cycle deletes owned window and font");
    flyoutResourceWindow=nullptr;return FeatureFlyoutResourceSample();
}
LRESULT FeatureFlyoutResourceCycles(int cycles) {
    // Small UI-owner batches preserve the existing two-second dispatch bound.
    for(int i=0;i<cycles;++i){FeatureFlyoutResourceOpen();FeatureFlyoutResourceClose();}
    return 1;
}
void FeatureFlyoutHeldResourceCycle() {
    OnUi(FeatureFlyoutResourceOpen);
    // Wait on the caller thread, allowing the actual UI owner to dispatch native
    // messages/timers while the focused window still exists. No nested UI pump.
    Sleep(1000);OnUi(FeatureFlyoutResourceClose);
}
struct FlyoutResourceObservation {FlyoutResourceSample last;bool within=true;Time elapsed=0;};
FlyoutResourceObservation FeatureFlyoutObserveResources(const FlyoutResourceSample* baseline=nullptr) {
    FlyoutResourceObservation result;auto started=GetTickCount64(),stableAt=started;
    OnUi(FeatureFlyoutResourceSample);auto previous=flyoutResourceSample;
    if(baseline&&!FeatureFlyoutResourcesWithin(*baseline,previous))result.within=false;
    do {
        Sleep(50);OnUi(FeatureFlyoutResourceSample);auto now=GetTickCount64();
        if(baseline&&!FeatureFlyoutResourcesWithin(*baseline,flyoutResourceSample))result.within=false;
        if(previous.gdi!=flyoutResourceSample.gdi||previous.user!=flyoutResourceSample.user||previous.handles!=flyoutResourceSample.handles)stableAt=now;
        previous=flyoutResourceSample;
        // This observes queued work both before the baseline and AFTER measured
        // waves. An increased sample remains a failure even if later released.
        if(now-started>=1000&&now-stableAt>=500){result.last=previous;result.elapsed=now-started;return result;}
    }while(GetTickCount64()-started<5000);
    Check(false,"native input resources settle within bounded observation window");return {};
}

// An owned, intentionally retained DC appears only after returning to the UI
// message loop. The real observation/predicate must reject it, then cleanup must
// restore counts. This is a control for the former post-wave sampling blind spot.
HDC flyoutDeferredControlDc{};UINT_PTR flyoutDeferredControlTimer{};bool flyoutDeferredControlFired=false;
void CALLBACK FeatureFlyoutDeferredControlTimer(HWND,UINT,UINT_PTR timer,DWORD) {
    Check(timer==flyoutDeferredControlTimer&&KillTimer(nullptr,timer)!=FALSE,"deferred control consumes its owned timer");
    flyoutDeferredControlTimer=0;flyoutDeferredControlDc=CreateCompatibleDC(nullptr);
    Check(flyoutDeferredControlDc!=nullptr,"deferred control allocates owned DC");flyoutDeferredControlFired=true;
}
void FeatureFlyoutDeferredResourceControl() {
    auto before=FeatureFlyoutObserveResources().last;
    OnUi(+[]()->LRESULT{
        Check(!flyoutDeferredControlDc&&!flyoutDeferredControlTimer&&!flyoutDeferredControlFired,"deferred control starts empty");
        flyoutDeferredControlTimer=SetTimer(nullptr,0,150,FeatureFlyoutDeferredControlTimer);
        Check(flyoutDeferredControlTimer!=0,"deferred control schedules owned timer");return 1;
    });
    auto deferredObservation=FeatureFlyoutObserveResources(&before);
    OnUi(+[]()->LRESULT{Check(flyoutDeferredControlFired&&flyoutDeferredControlDc&&!flyoutDeferredControlTimer,"deferred allocation ran after UI pumping");return 1;});
    Check(deferredObservation.last.gdi>before.gdi&&!deferredObservation.within,"real resource observer sees and rejects deferred retained DC");
    OnUi(+[]()->LRESULT{
        Check(DeleteDC(flyoutDeferredControlDc)!=FALSE,"deferred control deletes owned DC");flyoutDeferredControlDc=nullptr;
        flyoutDeferredControlFired=false;return 1;
    });
    auto cleaned=FeatureFlyoutObserveResources(&before);
    Check(cleaned.within&&FeatureFlyoutResourcesWithin(before,cleaned.last),"deferred control restores original closed-state resources");
    printf("FLYOUT deferred resource control PASS: GDI=%lu->%lu->%lu; delayed owned DC rejected by actual observer, timer consumed, DC deleted, original counts restored\n",before.gdi,deferredObservation.last.gdi,cleaned.last.gdi);fflush(stdout);
}
void FeatureFlyoutMediaQuiesce() {
    static bool acknowledged=false;const auto deadline=GetTickCount64()+4000;
    do {
        OnUi(+[]()->LRESULT{
            Check(!controller.config.media&&!controller.PolicyNow().media,"resource policy disables unrelated audio observation");
            for(auto& [id,p]:controller.PolicyNow().monitors){(void)id;Check(!p.enabled||p.media!=1,"resource targets do not force audio observation");}
            std::lock_guard lock(observationLock);Check(!mediaConfig.media,"resource worker configuration disables audio observation");
            for(auto& [id,p]:mediaConfig.monitors){(void)id;Check(!p.enabled||p.media!=1,"copied worker targets do not force audio observation");}
            acknowledged=mediaCompletedEpoch==observationEpoch&&!mediaCompletedNeeded;return 1;
        });
        if(acknowledged)break;Sleep(5);
    }while(GetTickCount64()<deadline);
    Check(acknowledged,"media worker acknowledges disabled epoch before fixed resource baseline");
    printf("FLYOUT resource media PASS: config/effective/copied policy disabled; actual worker completed current disabled epoch before warmup\n");fflush(stdout);
}
void FeatureTrayBorrowedResources() {
    const auto until=GetTickCount64()+4000;static bool idle=false;
    do {
        OnUi(+[]()->LRESULT{Maintain(GetTickCount64());std::lock_guard lock(maintenanceLock);idle=!trayOutstanding&&!maintenancePending.tray&&!maintenanceResultReady&&!maintenanceInTray;return 1;});
        if(idle)break;Sleep(5);
    }while(GetTickCount64()<until);
    Check(idle,"resource fixture drains initial tray work within bound");
    const auto baseline=FeatureFlyoutObserveResources().last;
    OnUi(+[]()->LRESULT{maintenanceTrayDelay=750;Tray();return 1;});
    const auto entered=GetTickCount64()+2000;while(!maintenanceInTray&&GetTickCount64()<entered)Sleep(1);
    Check(maintenanceInTray,"resource fixture observes active tray handoff");
    OnUi(FeatureFlyoutResourceSample);
    Check(FeatureFlyoutResourcesWithin(baseline,flyoutResourceSample),"active tray handoff allocates no GDI or USER resources");
    OnUi(+[]()->LRESULT{for(int replace=0;replace<32;++replace)Tray();std::lock_guard lock(maintenanceLock);Check(maintenancePending.icon==theme.smallIcon,"pending tray uses immutable host icon");return FeatureFlyoutResourceSample();});
    Check(FeatureFlyoutResourcesWithin(baseline,flyoutResourceSample),"pending replacements allocate no GDI or USER resources");
    maintenanceTrayDelay=0;
    const auto drained=GetTickCount64()+4000;idle=false;
    do {
        OnUi(+[]()->LRESULT{Maintain(GetTickCount64());std::lock_guard lock(maintenanceLock);idle=!trayOutstanding&&!maintenancePending.tray&&!maintenanceResultReady&&!maintenanceInTray;return 1;});
        if(idle)break;Sleep(5);
    }while(GetTickCount64()<drained);
    Check(idle,"stale and latest tray results converge within bound");
    const auto restored=FeatureFlyoutObserveResources(&baseline);
    Check(restored.within&&FeatureFlyoutResourcesWithin(baseline,restored.last),"tray completion retains original strict baseline");
    printf("FLYOUT tray borrowed ownership PASS: delayed active request, 32 pending replacements, stale result convergence, zero GDI/USER growth\n");fflush(stdout);
}
void FeatureFlyoutResources(bool inputControl) {
    // Match policy and maximum target cardinality in both input modes. The
    // preceding UI tests must not silently re-enable default media observation.
    OnUi(+[]()->LRESULT{
        SetupSessions();displays.clear();std::vector<std::string> ids;
        for(int i=0;i<64;++i){auto id="fixture-"+std::to_string(i);displays.push_back({id,"",L"Fixture display "+std::to_wstring(i+1),nullptr,{i*20,0,i*20+20,100},true});ids.push_back(id);}
        controller.Topology(ids,GetTickCount64());++topologyEpoch;
        SYSTEMTIME local{};GetLocalTime(&local);controller.SelectPolicy(GetTickCount64(),local.wHour*60+local.wMinute);InvalidateMedia();return 1;
    });
    FeatureFlyoutMediaQuiesce();
    FeatureTrayBorrowedResources();
    flyoutResourcePeak={};FeatureFlyoutHeldResourceCycle();OnUi(+[]()->LRESULT{return FeatureFlyoutResourceCycles(16);});
    auto before=FeatureFlyoutObserveResources().last;auto warmPeak=flyoutResourcePeak;
    auto transientLimit=before;
    // The focused native-input fixture exhibits short-lived closed-state USER
    // activity; the specific object's type and ownership remain unproven.
    // Calibrate its ceiling only during warmup; never raise it from measurements.
    // GDI and settled USER stay strict. The IME-disabled branch is strict for
    // every USER sample as well, without an arbitrary aggregate allowance.
    if(!inputControl)transientLimit.user=std::max(before.user,warmPeak.user);
    printf("FLYOUT resources %s baseline after 1 held + 16 rapid warmup cycles and settled native input: warm_USER_peak=%lu USER_transient_cap=%lu GDI=%lu USER=%lu handles=%lu\n",inputControl?"IME-disabled process control":"native-input",warmPeak.user,transientLimit.user,before.gdi,before.user,before.handles);fflush(stdout);
    for(int wave=0;wave<4;++wave){
        flyoutResourcePeak={};
        for(int batch=0;batch<3;++batch)OnUi(+[]()->LRESULT{return FeatureFlyoutResourceCycles(16);});
        OnUi(+[]()->LRESULT{return FeatureFlyoutResourceCycles(15);});FeatureFlyoutHeldResourceCycle();
        auto closedObservation=FeatureFlyoutObserveResources(&transientLimit);auto peak=flyoutResourcePeak;
        printf("FLYOUT resources %s wave=%d cycles=64 held=1 post_observation_ms=%llu closed peaks: GDI=%lu USER=%lu handles=%lu settled: GDI=%lu USER=%lu handles=%lu\n",inputControl?"IME-disabled process control":"native-input",wave+1,closedObservation.elapsed,peak.gdi,peak.user,peak.handles,closedObservation.last.gdi,closedObservation.last.user,closedObservation.last.handles);fflush(stdout);
        Check(closedObservation.within&&FeatureFlyoutResourcesWithin(transientLimit,peak)&&FeatureFlyoutResourcesWithin(before,closedObservation.last),"focused cycles and delayed observations obey fixed warmup ceiling, zero GDI/settled USER growth and bounded handles");
    }
    OnUi(SetupSessions);
}
void FeatureFlyout() {
    OnUi(StopSessions);Until([]{return snapshot.runs==0;},3000,"flyout fixture initial presentation cleanup");
    OnUi(FeatureFlyoutRetention);OnUi(FeatureFlyoutDpiThemeAndSettings);OnUi(FeatureFlyoutManyAndLifecycle);
    FeatureFlyoutResources(false);
    puts("PLATFORM flyout PASS: hidden native rows, stable selection, stale guards, drafts/focus, DPI/theme/high contrast, corresponding settings and 64-display scrolling/resource lifecycle; no visible accessibility acceptance claim");
}
