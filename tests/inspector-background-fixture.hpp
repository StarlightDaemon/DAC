// SPDX-License-Identifier: MIT
#pragma once
LRESULT InspectorCachedFixture() {
    SetupSessions();topologyPending=false;auto now=GetTickCount64();controller.config.nicknames["A"]="PRIVATE-INSPECTOR";
    controller.config.monitors["A"].hardware=true;faultCache["A"]=1;faultObservedAt=now;resettingFaults.insert("A");
    AppRule privateRule{2,false,false,"C:\\PRIVATE-PROCESS\\private-render.exe"};privateRule.targets={"A"};controller.config.rules={privateRule};controller.background.Reset(controller.backgroundEpoch,1);controller.background.Apply({controller.backgroundEpoch,now,{BackgroundMatch::Present}},now);
    displaySnapshot={topologyEpoch,now,false,std::make_shared<const std::vector<Display>>(displays),L"Injected cached OS retainedCatalog"};
    Check(controller.KeepAwake("A",now,15),"inspector fixture lease");controller.Manual("B",now,true,-1);
    auto before=controller.nodes;auto leases=controller.leases;auto generation=topologyEpoch;auto config=Serialize(controller.config);auto off=fakePowerOff.load(),on=fakePowerOn.load();
    ShowInspector("A");Check(inspector&&inspectorFont&&!IsWindowVisible(inspector),"inspector creates hidden native cached view");
    auto text=ControlText(inspector,InspectorDetail);Check(text.find(L"PRIVATE-INSPECTOR")!=std::wstring::npos&&text.find(L"permission only")!=std::wstring::npos&&text.find(L"not tested; unknown")!=std::wstring::npos&&text.find(L"marker retained")!=std::wstring::npos&&text.find(L"pending recovery reset: yes")!=std::wstring::npos,"inspector separates local identity, permission, unknown compatibility and cached quarantine/recovery");
    auto window=inspector,detail=GetDlgItem(inspector,InspectorDetail);SetFocus(detail);auto focus=GetFocus();SendMessageW(detail,EM_SETSEL,16,25);SendMessageW(detail,EM_LINESCROLL,0,5);auto line=SendMessageW(detail,EM_GETFIRSTVISIBLELINE,0,0);
    InspectorRefresh();Check(inspector==window&&GetFocus()==focus&&SendMessageW(detail,EM_GETFIRSTVISIBLELINE,0,0)==line,"inspector repaint retains read focus and scroll");
    std::reverse(displays.begin(),displays.end());InspectorRefresh();auto selected=static_cast<int>(SendDlgItemMessageW(inspector,InspectorList,LB_GETCURSEL,0,0));Check(selected>=0&&inspectorIds[selected]=="A","inspector stable selection survives reorder");
    for(auto& [id,node]:before)Check(FeaturePlatformNodeSame(node,controller.nodes.at(id)),"inspector opening/refresh has no presentation generation or idle side effect");
    Check(fakePowerOff==off&&fakePowerOn==on&&controller.leases.at("A").deadline==leases.at("A").deadline&&topologyEpoch==generation&&Serialize(controller.config)==config,"inspector does not invoke power or mutate authority/leases/topology");
    auto diagnostic=DiagnosticText();Check(diagnostic.find("PRIVATE-INSPECTOR")==std::string::npos&&diagnostic.find("PRIVATE-PROCESS")==std::string::npos&&diagnostic.find("private-render.exe")==std::string::npos&&diagnostic.find("Injected cached OS retainedCatalog")==std::string::npos&&diagnostic.find("DDC compatibility not tested")!=std::string::npos&&diagnostic.find("active=1")!=std::string::npos,"inspector diagnostic summary redacts active application paths/names and unapproved free text");
    Check(text.find(L"PRIVATE-PROCESS")!=std::wstring::npos&&SafeDiagnosticEvent(ReasonText(Reason::Background))==ReasonText(Reason::Background),"inspector may show configured application locally while background reason stays allowlisted");
    auto result=DisplayScanResult{topologyEpoch,configurationEpoch,now,true,displays};std::reverse(result.catalog.begin(),result.catalog.end());auto faultGeneration=faultEpoch;
    Check(AcceptDisplayScan(result)&&topologyEpoch==generation&&faultEpoch==faultGeneration&&controller.leases.at("A").deadline==leases.at("A").deadline,"unchanged OS rescan preserves topology, quarantine cache and leases");
    for(auto& [id,node]:before)Check(FeaturePlatformNodeSame(node,controller.nodes.at(id)),"unchanged rescan preserves all presentation generations");
    auto retainedCatalog=displaySnapshot.catalog;result.success=false;Check(!AcceptDisplayScan(result)&&displaySnapshot.stale&&displaySnapshot.catalog==retainedCatalog&&topologyEpoch==generation,"failed rescan retains previous immutable retainedCatalog visibly stale");
    bool complete=true;catalogMonitorInfoFail=true;Catalog(&complete);catalogMonitorInfoFail=false;Check(!complete,"per-monitor OS read failure cannot publish an incomplete authoritative topology");
    catalogDeviceInfoFail=true;Catalog(&complete);catalogDeviceInfoFail=false;Check(!complete,"per-path OS identity failure cannot publish an unidentified replacement topology");
    result.success=true;--result.configuration;Check(!AcceptDisplayScan(result)&&displaySnapshot.catalog==retainedCatalog,"configuration epoch rejects stale OS result");result.configuration=configurationEpoch;--result.topology;Check(!AcceptDisplayScan(result),"topology epoch rejects stale OS result");result.topology=topologyEpoch;
    unloading=true;Check(!AcceptDisplayScan(result)&&!RequestDisplayScan(),"shutdown rejects result and rescan requests");unloading=false;
    result.catalog[0].rect.right+=20;Check(AcceptDisplayScan(result)&&topologyEpoch==generation+1&&faultEpoch==faultGeneration+1&&!controller.Any(),"actual topology change applies established owner invalidation and recovery boundary");
    Check(CachedFault("A")&&resettingFaults.contains("A"),"OS rescan never deletes quarantine or pending recovery ownership");
    displaySnapshot.sampled=1;InspectorRefresh();Check(ControlText(inspector,InspectorStatus).find(L"stale / unknown")!=std::wstring::npos,"old sample explicitly shown stale");
    for(int dpi:{144,192,96}){RECT bounds{0,0,MulDiv(850,dpi,96),MulDiv(800,dpi,96)};SendMessageW(inspector,WM_DPICHANGED,MAKEWPARAM(dpi,dpi),reinterpret_cast<LPARAM>(&bounds));Check(inspector==window&&detail==GetDlgItem(inspector,InspectorDetail)&&inspectorLayout.dpi==dpi,"inspector DPI transitions retain native detail and owner");}
    for(int mode:{0,1,2}){testTheme=mode;ReadUiTheme();RefreshInspectorTheme();Check(inspectorFont&&inspector==window,"inspector light/dark/high contrast retains window");}testTheme=-1;ReadUiTheme();
    {auto savedDisplays=displays;for(int i=0;i<24;++i)displays.push_back({"inspector-scroll-"+std::to_string(i),"",L"Scroll fixture",nullptr,{0,0,100,100},false});InspectorRefresh();auto list=GetDlgItem(inspector,InspectorList);SendMessageW(list,LB_SETTOPINDEX,18,0);auto top=SendMessageW(list,LB_GETTOPINDEX,0,0);InspectorRefresh();Check(top>0&&SendMessageW(list,LB_GETTOPINDEX,0,0)==top,"inspector countdown refresh preserves list scrolling away from selected row");displays=std::move(savedDisplays);InspectorRefresh();}
    displays.erase(std::remove_if(displays.begin(),displays.end(),[](auto& d){return d.id=="A";}),displays.end());InspectorRefresh();Check(inspectorSelected=="A"&&ControlText(inspector,InspectorDetail).find(L"disconnected")!=std::wstring::npos,"inspector disconnect never silently selects peer");
    DestroyWindow(inspector);Check(!inspector&&!inspectorFont&&inspectorFontDeleted,"inspector releases native resources");resettingFaults.clear();SetupSessions();return 1;
}
LRESULT BackgroundSettingsFixture() {
    SetupSessions();topologyPending=false;if(editor)DestroyWindow(editor);ShowSettings();Check(editor,"background settings parent");ShowWorkspace();Check(workspace,"background rules workspace");
    auto before=WorkspaceRuleIdentity();ShowBackgroundRuleEditor();Check(backgroundEditor&&!IsWindowVisible(backgroundEditor),"background rule native draft editor");
    SetDlgItemTextW(backgroundEditor,1001,L"C:\\PRIVATE-APPLICATION\\render.exe");SendDlgItemMessageW(backgroundEditor,1005,LB_SETSEL,TRUE,0);
    {RECT bounds{0,0,1480,800};SendMessageW(backgroundEditor,WM_DPICHANGED,MAKEWPARAM(192,192),reinterpret_cast<LPARAM>(&bounds));
        for(int id:{1005,IDOK,IDCANCEL}){auto child=GetDlgItem(backgroundEditor,id);SetFocus(child);SendMessageW(backgroundEditor,WM_COMMAND,MAKEWPARAM(id,id==1005?LBN_SETFOCUS:BN_SETFOCUS),reinterpret_cast<LPARAM>(child));RECT client{},r{};GetClientRect(backgroundEditor,&client);GetWindowRect(child,&r);MapWindowPoints(nullptr,backgroundEditor,reinterpret_cast<POINT*>(&r),2);Check(r.top>=0&&r.bottom<=client.bottom,"background high-DPI clipped editor reveals keyboard target/apply/cancel focus");}}
    Check(ApplyBackgroundRuleDraft()&&workspaceRules.size()==1&&workspaceRules[0].kind==2&&!workspaceRules[0].global&&workspaceRules[0].targets.contains(backgroundTargetIds[0]),"background add requires explicit native stable target selection");
    DestroyWindow(backgroundEditor);workspaceRules[0].targets.insert("disconnected");ShowBackgroundRuleEditor(0);Check(std::find(backgroundTargetIds.begin(),backgroundTargetIds.end(),"disconnected")!=backgroundTargetIds.end(),"background edit retains disconnected stable target visibly");
    CheckDlgButton(backgroundEditor,1002,BST_UNCHECKED);Check(ApplyBackgroundRuleDraft()&&!workspaceRules[0].enabled&&workspaceRules[0].targets.contains("disconnected"),"background edit supports disablement without retargeting disconnected assignments");DestroyWindow(backgroundEditor);
    auto saved=WorkspaceRuleIdentity();ShowBackgroundRuleEditor(0);SetDlgItemTextW(backgroundEditor,1001,L"C:\\cancelled.exe");DestroyWindow(backgroundEditor);Check(saved==WorkspaceRuleIdentity(),"background Cancel preserves parent rules draft");
    ShowBackgroundRuleEditor(0);workspaceRules[0].executable="C:\\concurrent.exe";Check(!ApplyBackgroundRuleDraft(),"background stale nested draft cannot overwrite parent changes");DestroyWindow(backgroundEditor);
    Check(ApplyWorkspaceRules()&&draft.rules[0].kind==2,"background rules apply into ordinary main settings draft");
    auto diag=DiagnosticText();Check(diag.find("PRIVATE-APPLICATION")==std::string::npos,"background settings application path never enters diagnostics");
    auto portable=Portable(draft);Policy imported;std::map<std::string,std::string> mapping;for(auto& [id,p]:portable.monitors){(void)p;mapping[id]="mapped-"+id;}
    Check(MapImported(portable,mapping,imported)&&!imported.automatic&&imported.rules[0].targets.contains("mapped-disconnected"),"profile transfer explicitly maps disconnected rule targets and disarms automation");
    DestroyWindow(workspace);DestroyWindow(editor);Check(before!=saved,"rule draft fixture changes remained local");SetupSessions();return 1;
}
LRESULT BeginRejectedScanFixture() {
    SetupSessions();topologyPending=false;displayScanDelay=250;displayScanFail=false;auto began=GetTickCount64();
    Check(RequestDisplayScan()&&!RequestDisplayScan()&&GetTickCount64()-began<200,"OS scan requests coalesce without blocking input owner");++configurationEpoch;return 1;
}
LRESULT BackgroundSaveAndMediaFixture() {
    SetupSessions();topologyPending=false;auto originalPath=configPath;configPath=Executable()+L".background-schema5-settings";
    const std::string original="# exact schema4 background migration fixture\nversion=4\nautomatic=0\n";DWORD code=0;std::string bytes,error;auto backup=configPath+L".v4-backup";
    Check(WriteFileText(configPath,original,code)&&WriteFileText(backup,"mismatch",code),"schema4 migration fixture files");
    auto next=controller.config;AppRule r{2,false,false,"C:\\PRIVATE-BACKGROUND\\render.exe"};r.targets={"A"};next.rules={r};
    Check(!Save(next)&&ReadFileText(configPath,bytes,code,true)&&bytes==original,"schema5 mismatched backup blocks save and preserves original schema4 bytes");
    Check(DeleteFileW(Extended(backup).c_str()),"remove only owned mismatched migration fixture backup");
    controller.config.automatic=true;controller.config.media=true;controller.config.timeout=5;auto now=GetTickCount64();controller.SelectPolicy(now,0);controller.nodes.at("B")={now-6000,++controller.serial,State::Black,false,now-1000,false,-1};auto peer=controller.nodes.at("B");
    {std::lock_guard lock(observationLock);observation={now,true,false,false,{}};}
    auto mediaEpoch=observationEpoch;next=controller.config;next.rules={r};
    Check(Save(next)&&ReadFileText(backup,bytes,code,true)&&bytes==original,"schema5 verifies exact schema4 migration backup");
    Config parsed;Check(ReadFileText(configPath,bytes,code)&&Parse(bytes,parsed,error)&&parsed.sourceSchema==5&&parsed.rules[0].targets==r.targets,"schema5 native saved rule roundtrip");
    // Use the real production timer, including effective-policy and media paths.
    nextPoll=0;SendMessageW(ui,WM_TIMER,1,0);
    Check(observationEpoch==mediaEpoch&&observation.valid&&FeaturePlatformNodeSame(peer,controller.nodes.at("B")),"background-only Save and effective selection retain accepted media observation and unrelated automatic presentation");
    auto saved=Serialize(controller.config);auto changed=controller.config;changed.rules[0].enabled=false;
    {Handle held(CreateFileW(Extended(configPath).c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,0,nullptr));Check(held&&!Save(changed)&&Serialize(controller.config)==saved&&FeaturePlatformNodeSame(peer,controller.nodes.at("B")),"failed background settings save preserves rules media and unrelated presentation");}
    configPath=originalPath;SetupSessions();return 1;
}
LRESULT BeginFailedScanFixture(){displayScanDelay=0;displayScanFail=true;Check(RequestDisplayScan(),"OS failed scan fixture request");return 1;}
void InspectorBackgroundPlatform() {
    OnUi(InspectorCachedFixture);OnUi(BackgroundSettingsFixture);OnUi(BackgroundSaveAndMediaFixture);OnUi(StopSessions);Until([]{return snapshot.runs==0;},3000,"background media peer fixture cleanup");OnUi(FeatureDisplayMapGeometry);OnUi(FeatureDisplayMapNative);
    OnUi(BeginRejectedScanFixture);Time began=GetTickCount64();bool done=false;
    while(GetTickCount64()-began<10000){OnUi(+[]()->LRESULT{return 1;});{std::lock_guard lock(displayScanLock);done=!displayScanBusy;}if(done)break;Sleep(20);}
    Check(done,"bounded OS scan completion");OnUi(+[]()->LRESULT{Check(displaySnapshot.stale&&displaySnapshot.report.find(L"discarded")!=std::wstring::npos,"late native OS result rejected after configuration edit");return 1;});
    OnUi(BeginFailedScanFixture);began=GetTickCount64();done=false;
    while(GetTickCount64()-began<10000){{std::lock_guard lock(displayScanLock);done=!displayScanBusy;}if(done)break;Sleep(20);}
    Check(done,"failed native scan completion");OnUi(+[]()->LRESULT{Check(displaySnapshot.stale&&displaySnapshot.report.find(L"failed")!=std::wstring::npos,"OS worker failure retains visible stale report");displayScanFail=false;return 1;});
    BackgroundProcessFixture();
    puts("PLATFORM inspector/map/background PASS: cached local views, immutable OS rescan epochs/failures, native map/rule drafts, explicit profile mapping and bounded query-only workers; visible acceptance remains pending");
}
