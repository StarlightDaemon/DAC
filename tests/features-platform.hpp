// SPDX-License-Identifier: MIT
// Native integration fixtures; run only inside the existing private runner.
// Include after OnUi/Until and call FeaturePlatform with Initialize completed.
#pragma once

bool FeaturePlatformNodeSame(const Node& a,const Node& b) {
    return a.last==b.last&&a.generation==b.generation&&a.state==b.state&&a.manual==b.manual&&
        a.began==b.began&&a.sticky==b.sticky&&a.presentation==b.presentation&&
        a.powerRequested==b.powerRequested&&a.stageBegan==b.stageBegan;
}
LRESULT FeatureNamesAndMigration() {
    SetupSessions();auto savedPath=configPath;configPath=Executable()+L".features-settings";
    const auto backup=configPath+L".v3-backup";DeleteFileW(Extended(backup).c_str());
    DWORD code=0;std::string bytes,error;
    const std::string original="\xef\xbb\xbf# exact schema three fixture\nversion=3\nautomatic=0\nmonitor.41=1,-1\n";
    Check(WriteFileText(configPath,original,code),"feature isolated schema3 source");
    auto next=controller.config;next.nicknames={{"A","PRIVATE-HOME & \xe6\x9b\xb8\xe6\x96\x8e"},{"B","PRIVATE-HOME & \xe6\x9b\xb8\xe6\x96\x8e"},{"disconnected","Private retained"}};
    Check(WriteFileText(backup,"unverified backup",code)&&!Save(next,false),"feature unverified v3 backup prevents destructive migration");
    Check(ReadFileText(configPath,bytes,code,true)&&bytes==original,"feature failed migration leaves original schema3 bytes exact");
    DeleteFileW(Extended(backup).c_str());
    controller.Manual("B",GetTickCount64(),true,-1);auto peer=controller.nodes.at("B");
    Check(Save(next,false)&&ReadFileText(backup,bytes,code,true)&&bytes==original,"feature schema4 migration verifies exact v3 rollback backup");
    Config disk;Check(ReadFileText(configPath,bytes,code)&&Parse(bytes,disk,error)&&disk.sourceSchema==5&&disk.nicknames==next.nicknames,"feature migrated names persist with disconnected identities");
    Check(FeaturePlatformNodeSame(peer,controller.nodes.at("B")),"feature metadata save does not reset unrelated presentation");
    auto first=DisplayLabel(displays[0],controller.config,displays),second=DisplayLabel(displays[1],controller.config,displays);
    Check(first.find(L"PRIVATE-HOME")!=std::wstring::npos&&first.find(L"Display 1")!=std::wstring::npos&&second.find(L"Display 2")!=std::wstring::npos&&first!=second,"feature duplicate nickname labels retain visual numbered disambiguation");
    auto reordered=displays;std::reverse(reordered.begin(),reordered.end());
    Check(DisplayLabel(displays[0],controller.config,reordered).find(L"PRIVATE-HOME")!=std::wstring::npos,"feature nickname remains bound to identity across reorder");
    Check(MenuLabel(L"Desk & room\tCtrl+X")==L"Desk && room Ctrl+X","feature menu labels escape mnemonic and accelerator syntax");
    Record("PRIVATE-HOME & room","PRIVATE-HOME & room",0);
    auto diagnostic=DiagnosticText();
    Check(diagnostic.find("PRIVATE-HOME")==std::string::npos&&diagnostic.find("Private retained")==std::string::npos&&diagnostic.find("Display 1")!=std::string::npos,"feature diagnostics redact names including arbitrary event aliases");
    auto before=Serialize(controller.config);auto denied=controller.config;denied.nicknames["A"]="Changed while locked";
    {Handle held(CreateFileW(Extended(configPath).c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,0,nullptr));
        Check(!!held&&!Save(denied,false)&&Serialize(controller.config)==before&&FeaturePlatformNodeSame(peer,controller.nodes.at("B")),"feature failed native name replacement preserves configuration and peer session");}
    Check(ReadFileText(configPath,bytes,code)&&Parse(bytes,disk,error)&&disk.nicknames==next.nicknames,"feature failed native replacement preserves previously saved names");
    auto imported=controller.config;DisarmImportedConfig(imported);imported.nicknames["A"]="Imported metadata";
    Check(Save(imported,true,true)&&!controller.Any(),"feature explicit import or preference reset stops presentations even when only nickname metadata changes");
    configPath=savedPath;return 1;
}
LRESULT FeatureTargetControls() {
    SetupSessions();topologyPending=false;auto epoch=topologyEpoch;
    for(auto& [id,p]:controller.config.monitors){(void)id;p.saver=-1;}
    controller.Manual("B",GetTickCount64(),true,-1);Reconcile();auto b=controller.nodes.at("B");
    std::reverse(displays.begin(),displays.end());
    Check(ApplyDisplayControl("A",epoch,0)&&Running(controller.nodes.at("A").state)&&FeaturePlatformNodeSame(controller.nodes.at("B"),b),"feature start dispatch resolves stable identity after row reorder and preserves peer");
    Check(ApplyDisplayControl("A",epoch,1)&&!Running(controller.nodes.at("A").state)&&FeaturePlatformNodeSame(controller.nodes.at("B"),b),"feature stop dispatch resets only selected target");
    auto before=controller.nodes;
    Check(!ApplyDisplayControl("A",epoch+1,0)&&FeaturePlatformNodeSame(controller.nodes.at("A"),before.at("A")),"feature stale flyout epoch cannot activate a display");
    topologyPending=true;Check(!ApplyDisplayControl("A",epoch,0)&&FeaturePlatformNodeSame(controller.nodes.at("A"),before.at("A")),"feature pending topology rejects cached flyout action");topologyPending=false;
    Check(!ApplyDisplayControl("disconnected",epoch,0)&&!controller.nodes.contains("disconnected"),"feature removed row cannot create or redirect target");
    Check(ApplyDisplayControl("A",epoch,2,15)&&controller.leases.contains("A")&&FeaturePlatformNodeSame(controller.nodes.at("B"),b),"feature per-display control grants lease without resetting peer");
    auto deadline=controller.leases.at("A").deadline;
    Check(!ApplyDisplayControl("A",epoch+1,3)&&controller.leases.at("A").deadline==deadline,"feature stale cancellation cannot remove live lease");
    Check(ApplyDisplayControl("A",epoch,3)&&!controller.leases.contains("A")&&FeaturePlatformNodeSame(controller.nodes.at("B"),b),"feature cancel action affects only selected lease");
    controller.config.monitors["A"].enabled=false;before=controller.nodes;
    Check(!ApplyDisplayControl("A",epoch,0)&&!ApplyDisplayControl("A",epoch,2,15)&&FeaturePlatformNodeSame(controller.nodes.at("A"),before.at("A")),"feature disabled row cannot start or acquire lease");
    controller.config.monitors["A"].enabled=true;
    auto selected=std::find_if(displays.begin(),displays.end(),[](const Display& d){return d.id=="A";});selected->identified=false;
    Check(!ApplyDisplayControl("A",epoch,0)&&!ApplyDisplayControl("A",epoch,2,15),"feature unidentified row actions remain ineligible");
    Check(fakePowerOff==0&&fakePowerOn==0,"feature display control actions never perform hardware off or wake");
    return 1;
}
LRESULT FeatureNicknameDraft() {
    SetupSessions();ShowSettings();Check(editor&&GetDlgItem(editor,169),"feature nickname is editable in native settings draft");
    auto original=Serialize(controller.config);auto identity=editorDisplays[selection].id;
    SetDlgItemTextW(editor,169,L"Unsaved \x66f8\x658e & desk");
    SendMessageW(editor,WM_TIMER,1,0);Check(ControlText(editor,169)==L"Unsaved \x66f8\x658e & desk"&&Serialize(controller.config)==original,"feature status refresh retains unsaved nickname");
    testTheme=2;RefreshSettingsTheme();Check(ControlText(editor,169)==L"Unsaved \x66f8\x658e & desk"&&Serialize(controller.config)==original,"feature high contrast refresh retains unsaved nickname");testTheme=-1;RefreshSettingsTheme();
    RECT suggestion{100,100,1000,900};SendMessageW(editor,WM_DPICHANGED,MAKEWPARAM(192,192),reinterpret_cast<LPARAM>(&suggestion));
    Check(ControlText(editor,169)==L"Unsaved \x66f8\x658e & desk"&&editorLayout.dpi==192,"feature DPI refresh preserves nickname text and editor owner");
    StoreMonitorDraft();Check(draft.nicknames.at(identity)==Utf8(L"Unsaved \x66f8\x658e & desk")&&Serialize(controller.config)==original,"feature nickname store writes draft only by stable identity");
    SetDlgItemTextW(editor,169,L"Unsafe \x202e");Check(!ValidMonitorNumbers(),"feature native editor rejects bidi control before save");
    SetDlgItemTextW(editor,169,L"");StoreMonitorDraft();Check(!draft.nicknames.contains(identity),"feature empty nickname removes draft metadata and restores OS fallback");
    controller.config.nicknames[identity]="External saved name";auto external=Serialize(controller.config);
    SendMessageW(editor,WM_COMMAND,130,0);Check(Serialize(controller.config)==external&&saveFailure.find(L"changed elsewhere")!=std::wstring::npos,"feature stale nickname draft cannot overwrite external saved metadata");
    DestroyWindow(editor);return 1;
}
FILETIME FeatureCivilUtc(WORD year,WORD month,WORD day,WORD hour,WORD minute) {
    SYSTEMTIME time{};time.wYear=year;time.wMonth=month;time.wDay=day;time.wHour=hour;time.wMinute=minute;FILETIME result{};
    Check(SystemTimeToFileTime(&time,&result),"feature deterministic civil fixture");return result;
}
LRESULT FeatureExpiryInput() {
    int milliseconds=0;std::string resolved;std::wstring error;auto now=GetTickCount64();
    for(const auto& invalid:{L"",L"2030-02-30 12:00",L"2030-13-01 12:00",L"2030-01-01 24:00",L"2030-01-01 12:60",L"2030-1-01 12:00",L"2030-01-01T12:00",L"2030-01-01 12:00 trailing",L"2000-01-01 12:00"})
        Check(!ResolveLocalExpiry(invalid,now,milliseconds,resolved,error)&&!error.empty(),"feature absolute local expiry rejects malformed impossible past or distant input");
    TIME_ZONE_INFORMATION utc{};auto noon=FeatureCivilUtc(2030,1,1,12,0);
    Check(ResolveLocalExpiryInZone(L"2030-01-01 12:01",utc,noon,milliseconds,resolved,error)&&milliseconds==60000&&resolved.find("UTC 2030-01-01 12:01")!=std::string::npos&&error.empty(),"feature deterministic UTC expiry states exact UTC resolution");
    Check(ResolveLocalExpiryInZone(L"2030-01-02 12:00",utc,noon,milliseconds,resolved,error)&&milliseconds==86400000,"feature absolute expiry accepts exact 24-hour boundary");
    Check(!ResolveLocalExpiryInZone(L"2030-01-02 12:01",utc,noon,milliseconds,resolved,error)&&!ResolveLocalExpiryInZone(L"2030-01-01 12:00",utc,noon,milliseconds,resolved,error),"feature absolute expiry rejects beyond maximum and equality");
    Check(!ResolveLocalExpiryInZone(L"2030-02-30 12:00",utc,noon,milliseconds,resolved,error)&&!ResolveLocalExpiryInZone(L"2030-02-29 12:00",utc,noon,milliseconds,resolved,error),"feature absolute expiry never normalizes invalid calendar days");
    auto justBefore=FileTime(FileTicks(noon)-1);
    Check(ResolveLocalExpiryInZone(L"2030-01-01 12:00",utc,justBefore,milliseconds,resolved,error)&&milliseconds==1,"feature submillisecond civil expiry rounds up to future monotonic tick");
    TIME_ZONE_INFORMATION pacific{};pacific.Bias=480;pacific.StandardBias=0;pacific.DaylightBias=-60;
    pacific.DaylightDate.wMonth=3;pacific.DaylightDate.wDay=2;pacific.DaylightDate.wDayOfWeek=0;pacific.DaylightDate.wHour=2;
    pacific.StandardDate.wMonth=11;pacific.StandardDate.wDay=1;pacific.StandardDate.wDayOfWeek=0;pacific.StandardDate.wHour=2;
    auto spring=FeatureCivilUtc(2030,3,10,8,0);
    Check(!ResolveLocalExpiryInZone(L"2030-03-10 02:30",pacific,spring,milliseconds,resolved,error)&&error.find(L"does not exist")!=std::wstring::npos,"feature daylight-saving gap is rejected explicitly");
    Check(ResolveLocalExpiryInZone(L"2030-03-10 03:00",pacific,spring,milliseconds,resolved,error)&&milliseconds==7200000&&resolved.find("UTC 2030-03-10 10:00")!=std::string::npos,"feature unambiguous post-gap expiry uses daylight offset");
    auto autumn=FeatureCivilUtc(2030,11,3,7,0);
    Check(!ResolveLocalExpiryInZone(L"2030-11-03 01:30",pacific,autumn,milliseconds,resolved,error)&&error.find(L"occurs twice")!=std::wstring::npos,"feature daylight-saving fold is rejected explicitly");
    Check(ResolveLocalExpiryInZone(L"2030-11-03 02:30",pacific,autumn,milliseconds,resolved,error)&&milliseconds==12600000&&resolved.find("UTC 2030-11-03 10:30")!=std::string::npos,"feature unambiguous post-fold expiry uses standard offset");
    return 1;
}
void FeaturePlatform() {
    OnUi(StopSessions);Until([]{return snapshot.runs==0;},3000,"feature initial fixture cleanup");
    OnUi(FeatureNamesAndMigration);OnUi(StopSessions);Until([]{return snapshot.runs==0;},3000,"feature name fixture cleanup");OnUi(FeatureNicknameDraft);
    OnUi(+[]()->LRESULT{fakePowerOff=fakePowerOn=0;return 1;});OnUi(FeatureTargetControls);OnUi(FeatureExpiryInput);
    OnUi(StopSessions);Until([]{return snapshot.runs==0;},3000,"feature control fixture cleanup");OnUi(SetupSessions);
    puts("PLATFORM features PASS: private schema3 migration and transactional rollback, names/redaction, stable target controls, stale topology rejection and absolute expiry validation; hidden native fixtures only");
}
