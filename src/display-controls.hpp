// SPDX-License-Identifier: MIT
// In-process UI-owner actions. This is not a public command/IPC interface.
#pragma once

bool CurrentDisplayTarget(const std::string& id,Time epoch) {
    if(topologyPending||epoch!=topologyEpoch)return false;
    auto found=std::find_if(displays.begin(),displays.end(),[&](const Display& d){return d.id==id;});
    return found!=displays.end()&&found->identified&&controller.nodes.contains(id)&&!controller.unidentified.contains(id);
}
bool ApplyDisplayControl(const std::string& id,Time epoch,int action,int minutes=0,const std::string& expiry={}) {
    if(!CurrentDisplayTarget(id,epoch)||controller.Spanning())return false;
    auto now=GetTickCount64();
    if(action==0){if(!controller.Pref(id).enabled||controller.paused||controller.blocked)return false;controller.Manual(id,now);}
    else if(action==1)StopMonitor(id);
    else if(action==2){if(!controller.KeepAwake(id,now,minutes,expiry))return false;}
    else if(action==3){if(!controller.CancelKeepAwake(id,now))return false;}
    else return false;
    // Reconcile immediately hides/cancels stale owners before worker cleanup.
    ReconcileWake();nextPoll=0;return true;
}
uint64_t FileTicks(FILETIME value){return (uint64_t(value.dwHighDateTime)<<32)|value.dwLowDateTime;}
FILETIME FileTime(uint64_t value){return {static_cast<DWORD>(value),static_cast<DWORD>(value>>32)};}
bool SameCivil(const SYSTEMTIME& a,const SYSTEMTIME& b) {
    return a.wYear==b.wYear&&a.wMonth==b.wMonth&&a.wDay==b.wDay&&a.wHour==b.wHour&&a.wMinute==b.wMinute&&a.wSecond==b.wSecond;
}
// Round-trip both possible offsets. Zero matches is a DST gap, two is a fold;
// neither is silently assigned an arbitrary UTC instant.
bool ResolveLocalExpiryInZone(const std::wstring& text,const TIME_ZONE_INFORMATION& zone,FILETIME wallNow,int& durationMs,std::string& resolved,std::wstring& error) {
    error=L"Use a future local date/time: YYYY-MM-DD HH:MM, within 24 hours.";
    if(text.size()!=16||text[4]!=L'-'||text[7]!=L'-'||text[10]!=L' '||text[13]!=L':')return false;
    for(size_t i=0;i<text.size();++i)if(i!=4&&i!=7&&i!=10&&i!=13&&(text[i]<L'0'||text[i]>L'9'))return false;
    auto part=[&](size_t pos,size_t count){return static_cast<WORD>(std::stoi(text.substr(pos,count)));};
    SYSTEMTIME civil{};civil.wYear=part(0,4);civil.wMonth=part(5,2);civil.wDay=part(8,2);civil.wHour=part(11,2);civil.wMinute=part(14,2);
    FILETIME naive{};SYSTEMTIME checked{};
    if(civil.wYear<1601||civil.wYear>9999||!SystemTimeToFileTime(&civil,&naive)||!FileTimeToSystemTime(&naive,&checked)||!SameCivil(civil,checked))return false;
    std::set<uint64_t> candidates;
    for(auto bias:{zone.Bias+zone.StandardBias,zone.Bias+zone.DaylightBias}) {
        int64_t ticks=static_cast<int64_t>(FileTicks(naive))+static_cast<int64_t>(bias)*600000000;
        if(ticks<0)continue;auto utcFile=FileTime(static_cast<uint64_t>(ticks));SYSTEMTIME utc{},local{};
        if(FileTimeToSystemTime(&utcFile,&utc)&&SystemTimeToTzSpecificLocalTime(&zone,&utc,&local)&&SameCivil(civil,local))candidates.insert(static_cast<uint64_t>(ticks));
    }
    if(candidates.empty()){error=L"That local time does not exist because the clock moves forward. Choose another time.";return false;}
    if(candidates.size()!=1){error=L"That local time occurs twice because the clock moves back. Choose an unambiguous time or a duration.";return false;}
    auto target=*candidates.begin(),current=FileTicks(wallNow);
    if(target<=current||target-current>864000000000)return false;
    durationMs=static_cast<int>((target-current+9999)/10000);
    auto utcFile=FileTime(target);SYSTEMTIME utc{};FileTimeToSystemTime(&utcFile,&utc);char stamp[96]{};
    snprintf(stamp,sizeof(stamp),"%04u-%02u-%02u %02u:%02u local (UTC %04u-%02u-%02u %02u:%02u)",civil.wYear,civil.wMonth,civil.wDay,civil.wHour,civil.wMinute,utc.wYear,utc.wMonth,utc.wDay,utc.wHour,utc.wMinute);
    resolved=stamp;error.clear();return true;
}
bool ResolveLocalExpiry(const std::wstring& text,Time now,int& durationMs,std::string& resolved,std::wstring& error,Time* sampledOrigin=nullptr) {
    (void)now;DYNAMIC_TIME_ZONE_INFORMATION dynamic{};
    if(GetDynamicTimeZoneInformation(&dynamic)==TIME_ZONE_ID_INVALID){error=L"Windows time zone is unavailable.";return false;}
    TIME_ZONE_INFORMATION zone{};int year=0;
    if(text.size()<4||!Number(Utf8(text.substr(0,4)),year)||year<1601||year>9999||!GetTimeZoneInformationForYear(static_cast<USHORT>(year),&dynamic,&zone)){error=L"The date or time zone is unavailable.";return false;}
    if(dynamic.DynamicDaylightTimeDisabled){zone.DaylightDate={};zone.StandardDate={};zone.DaylightBias=zone.StandardBias;}
    FILETIME wallNow{};GetSystemTimeAsFileTime(&wallNow);if(sampledOrigin)*sampledOrigin=GetTickCount64();return ResolveLocalExpiryInZone(text,zone,wallNow,durationMs,resolved,error);
}
bool ApplyDisplayUntil(const std::string& id,Time epoch,const std::wstring& text,std::wstring& error) {
    if(!CurrentDisplayTarget(id,epoch)||!controller.LeaseRejection(id).empty()){error=L"Display changed or is ineligible. Stop manual/spanning protection first.";return false;}
    Time now=GetTickCount64();int duration=0;std::string resolved;
    if(!ResolveLocalExpiry(text,now,duration,resolved,error,&now))return false;
    if(now>UINT64_MAX-static_cast<Time>(duration)||!controller.KeepAwakeUntil(id,now,now+duration,resolved)){error=L"Keep-awake request was rejected.";return false;}
    ReconcileWake();nextPoll=0;return true;
}
