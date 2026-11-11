// SPDX-License-Identifier: MIT
// Pure F11 fixtures; included after the policy suite's Check helper.
#pragma once

void BackgroundPolicy() {
    AppRule path;path.kind=2;path.enabled=true;path.global=true;path.executable="C:\\Tasks\\render.exe";
    auto name=path;name.nameOnly=true;name.executable="render.exe";
    auto disabled=path;disabled.enabled=false;
    auto foreground=path;foreground.kind=0;
    BackgroundRequest request{7,{path,name,disabled,foreground}};
    auto process=[](uint32_t pid,const std::string& path,BackgroundScope scope=BackgroundScope::Eligible){
        BackgroundProcessIdentity row;row.pid=pid;row.app={path,BackgroundBasename(path),!path.empty()};row.scope=scope;row.name=row.app.name;return row;};
    auto present=process(10,"c:\\tasks\\RENDER.exe"),other=process(11,"C:\\Other\\render.exe");
    auto observation=MatchBackgroundCatalog(request,100,{present,other});
    Check(HasBackgroundRules(request.rules)&&observation.matches==std::vector<BackgroundMatch>{BackgroundMatch::Present,BackgroundMatch::Present,BackgroundMatch::Unknown,BackgroundMatch::Unknown},"background catalog aligns rules and excludes disabled/foreground kinds");
    observation=MatchBackgroundCatalog(request,200,{other});
    Check(observation.matches[0]==BackgroundMatch::Absent&&observation.matches[1]==BackgroundMatch::Present,"background full paths separate same-name applications; basename match remains explicit");
    auto second=present;second.pid=12;
    Check(MatchBackgroundCatalog(request,300,{present,second}).matches[0]==BackgroundMatch::Present&&MatchBackgroundCatalog(request,301,{second}).matches[0]==BackgroundMatch::Present&&MatchBackgroundCatalog(request,302,{}).matches[0]==BackgroundMatch::Absent,"background multiple live instances require all matching instances to exit");
    auto reused=process(present.pid,"C:\\Other\\replacement.exe");reused.name="render.exe";
    Check(MatchBackgroundCatalog(request,400,{reused}).matches[0]==BackgroundMatch::Absent,"background reused PID and stale Toolhelp name cannot inherit process identity");
    auto denied=process(20,"",BackgroundScope::Unknown);denied.name="unrelated.exe";
    Check(MatchBackgroundCatalog(request,500,{denied}).matches[0]==BackgroundMatch::Unknown,"background access denied never becomes absence using an untrusted snapshot basename");
    Check(MatchBackgroundCatalog(request,500,{denied,present},false).matches[0]==BackgroundMatch::Present,"background positive dominates unrelated denied processes and incomplete enumeration");
    Check(MatchBackgroundCatalog(request,500,{},false).matches[0]==BackgroundMatch::Unknown,"background failed enumeration is unknown rather than absence");
    auto crossSession=present;crossSession.scope=BackgroundScope::Excluded;auto crossUser=crossSession;crossUser.pid=21;
    auto ownedPid=crossSession;ownedPid.pid=22;auto ownedImage=crossSession;ownedImage.pid=23;auto ownedJob=crossSession;ownedJob.pid=24;
    Check(MatchBackgroundCatalog(request,600,{crossSession,crossUser,ownedPid,ownedImage,ownedJob}).matches[0]==BackgroundMatch::Absent,"background explicit cross-session cross-user own-PID own-image and owned-job exclusions never match");
    auto ownershipUnknown=present;ownershipUnknown.scope=BackgroundScope::Unknown;
    Check(MatchBackgroundCatalog(request,601,{ownershipUnknown}).matches[0]==BackgroundMatch::Unknown,"background unknown owned-job membership cannot become an eligible positive");
    std::vector<BackgroundProcessIdentity> crowded(kBackgroundProcessLimit,other);
    crowded.back()=present;
    Check(MatchBackgroundCatalog(request,700,crowded).matches[0]==BackgroundMatch::Present,"background catalog includes final bounded process entry");
    crowded.push_back(present);crowded[kBackgroundProcessLimit-1]=other;
    Check(MatchBackgroundCatalog(request,701,crowded).matches[0]==BackgroundMatch::Unknown,"background truncated process catalog cannot infer absence");
    auto oversized=request;oversized.rules.resize(33,path);
    Check(!HasBackgroundRules(oversized.rules)&&MatchBackgroundCatalog(oversized,701,{present}).matches.size()==32,"background runtime rule bound cannot scan oversized policy");
    Check(!HasBackgroundRules({disabled,foreground}),"background no scans requested for disabled and foreground-only policy");

    BackgroundTracker tracker;tracker.Reset(7,4);
    Check(!tracker.Active(0,0)&&tracker.Uncertain(0,0),"background starts uncertain without inhibition");
    Check(tracker.Apply({7,100,{BackgroundMatch::Present,BackgroundMatch::Absent,BackgroundMatch::Unknown,BackgroundMatch::Unknown}},100)&&tracker.Active(0,100)&&!tracker.Uncertain(0,100)&&!tracker.Active(1,100),"background accepts matching epoch present and absent snapshot");
    Check(tracker.Apply({7,1000,{BackgroundMatch::Unknown,BackgroundMatch::Unknown,BackgroundMatch::Unknown,BackgroundMatch::Unknown}},1000)&&tracker.Active(0,5100)&&tracker.Uncertain(0,1000)&&!tracker.Active(0,5101),"background unknown retains at most five seconds since the actual positive");
    Check(tracker.Apply({7,5000,{BackgroundMatch::Unknown,BackgroundMatch::Unknown,BackgroundMatch::Unknown,BackgroundMatch::Unknown}},5000)&&!tracker.Active(0,5101),"background repeated unknown never self-refreshes positive grace");
    Check(!tracker.Apply({8,5001,{BackgroundMatch::Present,BackgroundMatch::Present,BackgroundMatch::Present,BackgroundMatch::Present}},5001)&&!tracker.Active(0,5101),"background rejects mismatched epoch observations");
    Check(!tracker.Apply({7,6000,{BackgroundMatch::Present,BackgroundMatch::Present,BackgroundMatch::Present,BackgroundMatch::Present}},5001),"background rejects future observation timestamps");
    Check(!tracker.Apply({7,1,{BackgroundMatch::Present,BackgroundMatch::Present,BackgroundMatch::Present,BackgroundMatch::Present}},6000),"background rejects stale observations without refreshing grace");
    Check(!tracker.Apply({7,4000,{BackgroundMatch::Present,BackgroundMatch::Present,BackgroundMatch::Present,BackgroundMatch::Present}},5001),"background rejects out-of-order observations");
    Check(!tracker.Apply({7,5001,{BackgroundMatch::Present}},5001),"background rejects mismatched rule vector without partial mutation");
    Check(tracker.Apply({7,6000,{BackgroundMatch::Present,BackgroundMatch::Present,BackgroundMatch::Unknown,BackgroundMatch::Unknown}},6000)&&tracker.Active(0,6000),"background later fresh positive recovers inhibition");
    Check(tracker.Apply({7,6001,{BackgroundMatch::Absent,BackgroundMatch::Unknown,BackgroundMatch::Unknown,BackgroundMatch::Unknown}},6001)&&!tracker.Active(0,6001)&&!tracker.Uncertain(0,6001)&&tracker.Active(1,6001),"background fresh absence removes only its own rule promptly");
    Check(tracker.Uncertain(0,11002)&&!tracker.Active(1,11001),"background stale absence becomes uncertain and old positives expire independently");
    tracker.Reset(8,1);
    Check(tracker.Epoch()==8&&!tracker.Active(0,6001)&&tracker.Uncertain(0,6001)&&!tracker.Apply({7,6001,{BackgroundMatch::Present}},6001),"background policy/profile epoch reset revokes all prior positive authority");
    Check(tracker.Apply({8,0,{BackgroundMatch::Present}},0)&&tracker.Active(0,0)&&!tracker.Active(0,5001)&&!tracker.Active(1,0)&&tracker.Uncertain(1,0),"background handles monotonic zero and out-of-range indices safely");
    puts("POLICY background: bounded path/name catalogs, multiple instances, PID reuse, access failures, explicit exclusions, epoch/timestamp rejection and five-second grace PASS");
}
