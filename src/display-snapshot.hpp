// SPDX-License-Identifier: MIT
#pragma once
// Included inside dac after Catalog. Only the UI publishes the accepted catalog.
// Rectangles are signed Windows desktop pixels (PMv2), not per-monitor DIPs.
struct DisplaySnapshot {
    Time topology=0,sampled=0;bool stale=true;
    std::shared_ptr<const std::vector<Display>> catalog=std::make_shared<const std::vector<Display>>();
    std::wstring report=L"Not sampled";
};
struct DisplayScanResult {Time topology=0,configuration=0,at=0;bool success=false;std::vector<Display> catalog;};
DisplaySnapshot displaySnapshot;
bool DisplaySnapshotIsStale(Time now){return displaySnapshot.stale||!displaySnapshot.sampled||now<displaySnapshot.sampled||now-displaySnapshot.sampled>30000;}
std::mutex displayScanLock;std::condition_variable displayScanChanged;std::thread displayScanWorker;
bool displayScanStop=false,displayScanPending=false,displayScanReady=false,displayScanBusy=false;
Time displayScanTopology=0,displayScanConfiguration=0,nextCatalogAttempt=0;
std::shared_ptr<const DisplayScanResult> displayScanResult;
#ifdef DAC_HARNESS
std::atomic<unsigned> displayScanCalls{};
std::atomic<DWORD> displayScanDelay{};std::atomic<bool> displayScanFail{};
#endif
bool SameDisplayTopology(const std::vector<Display>& a,const std::vector<Display>& b) {
    if(a.size()!=b.size())return false;
    for(auto& x:a){auto y=std::find_if(b.begin(),b.end(),[&](const Display& d){return d.id==x.id;});
        if(y==b.end()||x.alias!=y->alias||x.identified!=y->identified||x.handle!=y->handle||x.rotation!=y->rotation||x.technology!=y->technology||x.targetAvailable!=y->targetAvailable||!EqualRect(&x.rect,&y->rect))return false;}return true;
}
void DisplayScanMain() {
    std::unique_lock lock(displayScanLock);
    while(true){displayScanChanged.wait(lock,[]{return displayScanStop||displayScanPending;});if(displayScanStop)break;
        auto result=std::make_shared<DisplayScanResult>();result->topology=displayScanTopology;result->configuration=displayScanConfiguration;displayScanPending=false;
        lock.unlock();result->at=GetTickCount64();
#ifdef DAC_HARNESS
        ++displayScanCalls;if(auto delay=displayScanDelay.load())Sleep(delay);
#endif
        try{result->catalog=Catalog(&result->success);}catch(...){result->success=false;}
#ifdef DAC_HARNESS
        if(displayScanFail)result->success=false;
#endif
        lock.lock();if(!displayScanStop){displayScanResult=std::move(result);displayScanReady=true;}
    }
}
bool RequestDisplayScan() {
    if(unloading||!ui)return false;
    std::lock_guard lock(displayScanLock);if(displayScanStop||displayScanBusy)return false;
    displayScanTopology=topologyEpoch;displayScanConfiguration=configurationEpoch;displayScanBusy=true;displayScanPending=true;
    displaySnapshot.stale=true;displaySnapshot.report=L"OS-only rescan in progress… (no hardware queries)";
    if(!displayScanWorker.joinable())try{displayScanWorker=std::thread(DisplayScanMain);}catch(...){displayScanBusy=displayScanPending=false;displaySnapshot.report=L"OS rescan worker unavailable; previous snapshot is stale";return false;}
    displayScanChanged.notify_one();return true;
}
void StopDisplayScan() {
    {std::lock_guard lock(displayScanLock);displayScanStop=true;displayScanPending=false;displayScanChanged.notify_one();}
    if(displayScanWorker.joinable())displayScanWorker.join();
    std::lock_guard lock(displayScanLock);displayScanResult.reset();displayScanBusy=displayScanReady=false;
}
std::string DisplayDiagnosticSummary() {
    auto now=GetTickCount64();std::ostringstream out;bool stale=DisplaySnapshotIsStale(now);
    out<<"OS snapshot: generation="<<displaySnapshot.topology<<" at="<<displaySnapshot.sampled<<" stale="<<stale<<" rescanPending="<<displayScanBusy<<" displays="<<displays.size()<<"; DDC compatibility not tested; observed hardware state unknown\n";
    size_t enabled=0,active=0,uncertain=0;auto& rules=controller.PolicyNow().rules;for(size_t i=0;i<rules.size();++i)if(rules[i].enabled&&rules[i].kind==2){++enabled;if(controller.background.Active(i,now))++active;if(controller.background.Uncertain(i,now))++uncertain;}
    out<<"Background rules: enabled="<<enabled<<" active="<<active<<" uncertain="<<uncertain<<" graceMaxMs="<<kBackgroundGraceMs<<"\n";return out.str();
}
