// SPDX-License-Identifier: MIT
// Run only through dac-test-runner's private desktop. Opens query handles only.
#pragma once

void BackgroundProcessFixture() {
    BackgroundProcessWorker worker;BackgroundObservation processSample;
    AppRule disabled;disabled.kind=2;disabled.enabled=false;disabled.global=true;disabled.executable=Utf8(Executable());
    Check(worker.Request({1,{disabled}})&&!worker.Take(processSample),"background disabled-only request performs no scan or publication");
    AppRule own=disabled;own.enabled=true;
    const Time requested=GetTickCount64();
    Check(worker.Request({2,{own}}),"background worker accepts bounded request");
    Time began=GetTickCount64();bool received=false;
    while(GetTickCount64()-began<10000){if(worker.Take(processSample)){received=true;break;}Sleep(10);}
    Check(received&&processSample.epoch==2&&processSample.at>=requested&&processSample.at<=GetTickCount64()&&processSample.matches.size()==1&&processSample.matches[0]!=BackgroundMatch::Present,"background native scanner excludes own PID and same executable siblings");
    Check(processSample.inspected<=kBackgroundProcessLimit,"background native enumeration respects process bound");
    printf("BACKGROUND native scan: inspected=%zu duration=%llu us result=%d (query-only current user/session; no launched applications)\n",processSample.inspected,processSample.durationMicros,static_cast<int>(processSample.matches[0]));
    const auto previousAt=processSample.at;
    for(Time epoch=3;epoch<=66;++epoch)Check(worker.Request({epoch,{own}}),"background worker coalesces replacement requests");
    began=GetTickCount64();received=false;
    while(GetTickCount64()-began<10000){if(worker.Take(processSample)&&processSample.epoch==66){received=true;break;}Sleep(10);}
    Check(received&&processSample.at>=previousAt+1000&&processSample.matches[0]!=BackgroundMatch::Present,"background latest epoch wins with at most one scan per second");
    Check(worker.Request({67,{disabled}})&&!worker.Take(processSample),"background disabled policy invalidates queued observations");
    worker.Stop();worker.Stop();
    Check(!worker.Request({68,{own}}),"background stopped worker cannot restart");

    // The runner is an already-running, retained parent process in our user and
    // session. It supplies a deterministic external identity without launching
    // an arbitrary application or depending on the visible desktop's programs.
    DWORD parentPid=0;Handle processCatalogHandle(CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0));PROCESSENTRY32W entry{};entry.dwSize=sizeof(entry);
    Check(processCatalogHandle&&Process32FirstW(processCatalogHandle,&entry),"background fixture enumerates existing runner parent");
    do{if(entry.th32ProcessID==GetCurrentProcessId()){parentPid=entry.th32ParentProcessID;break;}}while(Process32NextW(processCatalogHandle,&entry));
    Handle parent(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION|SYNCHRONIZE,FALSE,parentPid));
    std::vector<wchar_t> path(32768);DWORD length=static_cast<DWORD>(path.size());
    Check(parent&&QueryFullProcessImageNameW(parent,0,path.data(),&length)&&length&&length<path.size(),"background fixture retains runner process identity");
    AppRule parentRule=own;parentRule.executable=Utf8(std::wstring(path.data(),length));
    Check(!ExecutableEqual(parentRule.executable,own.executable),"background fixture is hosted by separate private-desktop runner");
    std::atomic<BackgroundScope> liveScope{BackgroundScope::Eligible};std::atomic<unsigned> callbacks{0};
    BackgroundProcessWorker scoped([&](HANDLE process){if(GetProcessId(process)==parentPid)++callbacks;return liveScope.load();});
    for(const auto scope:{BackgroundScope::Eligible,BackgroundScope::Unknown,BackgroundScope::Excluded}) {
        liveScope=scope;const Time epoch=100+static_cast<Time>(scope);
        Check(scoped.Request({epoch,{parentRule}}),"background live scope callback request");
        began=GetTickCount64();received=false;
        while(GetTickCount64()-began<10000){if(scoped.Take(processSample)&&processSample.epoch==epoch){received=true;break;}Sleep(10);}
        Check(received&&processSample.matches.size()==1&&callbacks>0,"background callback receives retained eligible process handle");
        if(scope==BackgroundScope::Eligible)Check(processSample.matches[0]==BackgroundMatch::Present,"background current-user/session parent identity produces positive processSample");
        else if(scope==BackgroundScope::Unknown)Check(processSample.matches[0]==BackgroundMatch::Unknown,"background failed live ownership query never produces positive or false absence");
        else Check(processSample.matches[0]!=BackgroundMatch::Present,"background current registry exclusion overrides positive executable match");
    }
    scoped.Stop();
    puts("BACKGROUND native worker PASS: lazy disabled policy, current executable exclusion, timestamped bounded scan, coalesced epochs, one-Hz throttle, live registry callback and joined shutdown");
}
