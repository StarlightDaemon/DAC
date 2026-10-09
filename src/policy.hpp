// SPDX-License-Identifier: MIT
// Copyright (c) 2026 StarlightDaemon
// Selectively ported from verified DAC R01C; see docs/SOURCE_INVENTORY.md.
#pragma once
#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <vector>
#include <random>
#include <cmath>
#include <array>
#include <deque>
#include <condition_variable>
#include <chrono>
#ifdef _WIN32
#include <windows.h>
#endif

namespace dac {
constexpr char kVersion[]="0.1.0"; // Independent standalone product version.
struct IntegrationSettings { bool startupSetup=false; int trayAction=0,appearance=0; };
std::string SerializeIntegration(const IntegrationSettings& value) {
    return "version=1\nstartupSetup="+std::to_string(value.startupSetup)+"\ntrayAction="+std::to_string(value.trayAction)+"\nappearance="+std::to_string(value.appearance)+"\n";
}
bool ParseIntegration(const std::string& text,IntegrationSettings& output) {
    if(text.size()>1024)return false;
    IntegrationSettings next;std::istringstream input(text);std::string line;std::set<std::string> keys;
    while(std::getline(input,line)) {
        if(!line.empty()&&line.back()=='\r')line.pop_back();if(line.empty())continue;
        auto split=line.find('=');if(split==std::string::npos)return false;
        auto key=line.substr(0,split),value=line.substr(split+1);if(!keys.insert(key).second||value.size()!=1)return false;
        if(key=="version"){if(value!="1")return false;}
        else if(key=="startupSetup"){if(value!="0"&&value!="1")return false;next.startupSetup=value=="1";}
        else if(key=="trayAction"){if(value<"0"||value>"2")return false;next.trayAction=value[0]-'0';}
        else if(key=="appearance"){if(value<"0"||value>"2")return false;next.appearance=value[0]-'0';}
        else return false;
    }
    if(keys.size()!=4)return false;output=next;return true;
}
using Time = uint64_t;
struct Preference {
    bool enabled=true; int saver=-1; // -1: native black
    int timeout=0,input=-1,media=-1,blackAfter=0;
    bool hardware=false; int powerAfter=0; bool fullscreen=false;
    std::string custom{},folder{};int slideSeconds=30,placement=0,background=0;bool shuffle=false,recursive=false;
    int dim=0,fadeMs=500,dimSeconds=5,sceneTheme=0,sceneSeconds=300,rotateSeconds=0;bool batteryBlack=false;
};
std::string Canonical(std::string s) {
    if(s.find('\\')!=std::string::npos) for(char& c:s) if(c>='A'&&c<='Z') c+=32;
    return s;
}
struct AppIdentity {std::string path,name;bool known=false;};
struct AppRule {int kind=0;bool global=false,nameOnly=false;std::string executable;};
std::string Lower(std::string s) {for(auto& ch:s)if(ch>='A'&&ch<='Z')ch+=32;return s;}
bool ExecutableEqual(const std::string& first,const std::string& second) {
#ifdef _WIN32
    int a=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,first.data(),static_cast<int>(first.size()),nullptr,0),b=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,second.data(),static_cast<int>(second.size()),nullptr,0);
    if(!a||!b)return false;std::wstring x(a,L'\0'),y(b,L'\0');MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,first.data(),static_cast<int>(first.size()),x.data(),a);MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,second.data(),static_cast<int>(second.size()),y.data(),b);
    return CompareStringOrdinal(x.data(),a,y.data(),b,TRUE)==CSTR_EQUAL;
#else
    return Lower(first)==Lower(second);
#endif
}
bool MatchApp(const AppIdentity& app,const std::string& pattern,bool nameOnly) {
    return app.known&&!pattern.empty()&&ExecutableEqual(nameOnly?app.name:app.path,pattern);
}
bool RuleMatch(const AppRule& rule,const AppIdentity& app) {return MatchApp(app,rule.executable,rule.nameOnly);}
struct Policy {
    int timeout=60, poll=1000, padding=0;
    bool automatic=false, perInput=true, media=true, perMedia=true, muted=false, debug=false;
    bool hotkeys=false;int currentKey=11,allKey=10;
    bool controllerInput=false;
    std::vector<AppRule> rules;
    std::map<std::string,Preference> monitors;
};
struct Profile {std::string name,app;int trigger=0,start=0,end=0;bool nameOnly=false;Policy policy;};
struct Config : Policy {int sourceSchema=3;std::string manualProfile;std::vector<Profile> profiles;};
void DisarmImportedConfig(Config& config) {
    config.automatic=false;
    for(auto& [id,p]:config.monitors){(void)id;p.hardware=false;p.powerAfter=0;}
    for(auto& profile:config.profiles){profile.policy.automatic=false;for(auto& [id,p]:profile.policy.monitors){(void)id;p.hardware=false;p.powerAfter=0;}}
}
void Validate(Config& c) {
    c.timeout=std::clamp(c.timeout,5,3600); c.poll=std::clamp(c.poll,250,10000);
    c.padding=std::clamp(c.padding,0,1024);
    for(auto& [id,p]:c.monitors) {
        (void)id; p.saver=std::clamp(p.saver,-1,9);
        p.timeout=p.timeout==0?0:std::clamp(p.timeout,5,3600);
        p.input=std::clamp(p.input,-1,3);p.media=std::clamp(p.media,-1,1);
        p.blackAfter=p.blackAfter==0?0:std::clamp(p.blackAfter,5,86400);
        p.powerAfter=p.powerAfter==0?0:std::clamp(p.powerAfter,5,86400);
        p.dim=std::clamp(p.dim,0,100);p.fadeMs=std::clamp(p.fadeMs,0,10000);p.dimSeconds=std::clamp(p.dimSeconds,1,300);
        p.sceneTheme=std::clamp(p.sceneTheme,0,2);p.sceneSeconds=std::clamp(p.sceneSeconds,5,86400);p.rotateSeconds=p.rotateSeconds?std::clamp(p.rotateSeconds,5,3600):0;
        p.slideSeconds=std::clamp(p.slideSeconds,5,3600);p.placement=std::clamp(p.placement,0,5);p.background=std::clamp(p.background,0,0xffffff);
    }
    c.currentKey=std::clamp(c.currentKey,1,12);c.allKey=std::clamp(c.allKey,1,12);
}
std::string Trim(std::string s) {
    auto a=s.find_first_not_of(" \t\r\n"), b=s.find_last_not_of(" \t\r\n");
    return a==std::string::npos?"":s.substr(a,b-a+1);
}
bool Number(const std::string& s,int& n) {
    if(s.empty() || s.size()>10) return false;
    int64_t value=0; size_t i=s[0]=='-'?1:0; if(i==s.size()) return false;
    for(;i<s.size();++i) { if(s[i]<'0'||s[i]>'9') return false; value=value*10+s[i]-'0'; if(value>2147483647) return false; }
    n=static_cast<int>(s[0]=='-'?-value:value); return true;
}
std::string Hex(const std::string& s) {
    const char* digits="0123456789abcdef"; std::string out;
    for(unsigned char ch:s) { out+=digits[ch>>4]; out+=digits[ch&15]; } return out;
}
bool ValidUtf8(const std::string& s) {
    for(size_t i=0;i<s.size();) {unsigned char c=s[i++];if(c<0x80){if(!c)return false;continue;}
        unsigned n=c>=0xc2&&c<=0xdf?1:c>=0xe0&&c<=0xef?2:c>=0xf0&&c<=0xf4?3:0;if(!n||i+n>s.size())return false;
        uint32_t value=c&((1u<<(6-n))-1);for(unsigned j=0;j<n;++j){unsigned char t=s[i++];if((t&0xc0)!=0x80)return false;value=(value<<6)|(t&63);}
        if((n==1&&value<0x80)||(n==2&&value<0x800)||(n==3&&value<0x10000)||value>0x10ffff||(value>=0xd800&&value<=0xdfff))return false;
    }return true;
}
bool Unhex(const std::string& s,std::string& out,size_t limit=8192) {
    if(s.empty()||s.size()%2||s.size()>limit) return false; out.clear();
    auto digit=[](char c) { return c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:-1; };
    for(size_t i=0;i<s.size();i+=2) { int a=digit(s[i]),b=digit(s[i+1]); if(a<0||b<0||!(a*16+b)) return false; out+=char(a*16+b); } return ValidUtf8(out);
}
std::string Serialize(Config c) {
    Validate(c); std::ostringstream o;
    o<<"version=3\ntimeout="<<c.timeout<<"\npoll="<<c.poll<<"\npadding="<<c.padding
     <<"\nautomatic="<<c.automatic<<"\nperInput="<<c.perInput<<"\nmedia="<<c.media
     <<"\nperMedia="<<c.perMedia<<"\nmuted="<<c.muted<<"\ndebug="<<c.debug<<'\n'
     <<"hotkeys="<<c.hotkeys<<"\ncurrentKey="<<c.currentKey<<"\nallKey="<<c.allKey<<'\n';
    o<<"controllerInput="<<c.controllerInput<<'\n'<<"manualProfile="<<Hex(c.manualProfile)<<'\n';
    for(size_t i=0;i<c.rules.size();++i){auto& r=c.rules[i];o<<"rule."<<i<<'='<<r.kind<<'|'<<r.global<<'|'<<r.nameOnly<<'|'<<Hex(r.executable)<<'\n';}
    for(auto& profile:c.profiles){Config body;static_cast<Policy&>(body)=profile.policy;
        o<<"profile."<<Hex(profile.name)<<'='<<profile.trigger<<'|'<<profile.start<<'|'<<profile.end<<'|'<<profile.nameOnly<<'|'<<Hex(profile.app)<<'|'<<Hex(Serialize(body))<<'\n';}
    for(auto& [id,p]:c.monitors) {
        o<<"next."<<Hex(id)<<'='<<p.dim<<','<<p.fadeMs<<','<<p.dimSeconds<<','<<p.batteryBlack<<','<<p.sceneTheme<<','<<p.sceneSeconds<<','<<p.rotateSeconds<<'\n';
        o<<"monitor."<<Hex(id)<<'='<<p.enabled<<','<<p.saver<<'\n';
        o<<"policy."<<Hex(id)<<'='<<p.timeout<<','<<p.input<<','<<p.media<<','<<p.blackAfter<<','<<p.hardware<<','<<p.powerAfter<<','<<p.fullscreen<<'\n';
        o<<"assets."<<Hex(id)<<'='<<Hex(p.custom)<<'|'<<Hex(p.folder)<<'|'<<p.slideSeconds<<'|'<<p.placement<<'|'<<p.shuffle<<'|'<<p.recursive<<'|'<<p.background<<'\n';
    }
    return o.str();
}
bool Parse(const std::string& text,Config& result,std::string& error,int depth=0) {
    if(text.size()>262144) { error="Configuration exceeds 256 KiB"; return false; }
    Config c; bool version=false,modern=false; std::set<std::string> keys; std::istringstream in(text); std::string line;
    while(std::getline(in,line)) {
        line=Trim(line); if(line.empty()||line[0]=='#') continue;
        auto eq=line.find('='); if(eq==std::string::npos) { error="Expected key=value"; return false; }
        auto k=Trim(line.substr(0,eq)),v=Trim(line.substr(eq+1)); int n=0;
        if(!keys.insert(k).second) { error="Duplicate key"; return false; }
        if(k=="manualProfile"){modern=true;if(!v.empty()&&!Unhex(v,c.manualProfile)){error="Invalid selected profile";return false;}continue;}
        if(k.starts_with("rule.")||k.starts_with("profile.")) {
            if(v.ends_with('|')){error="Trailing rule/profile separator";return false;}
            modern=true;std::istringstream fields(v);std::string field;std::vector<std::string> values;while(std::getline(fields,field,'|'))values.push_back(field);
            if(k.starts_with("rule.")){AppRule r;int kind=0,global=0,nameOnly=0,index=0;
                if(c.rules.size()>=32||values.size()!=4||!Number(k.substr(5),index)||index!=static_cast<int>(c.rules.size())||!Number(values[0],kind)||!Number(values[1],global)||!Number(values[2],nameOnly)||kind<0||kind>1||global<0||global>1||nameOnly<0||nameOnly>1||!Unhex(values[3],r.executable)){error="Invalid application rule";return false;}
                r.kind=kind;r.global=global;r.nameOnly=nameOnly;if((!r.nameOnly&&(r.executable.size()<3||r.executable[1]!=':'||r.executable[2]!='\\'))||(r.nameOnly&&r.executable.find_first_of("/\\")!=std::string::npos)){error="Rule requires a full drive path or explicit name-only match";return false;}c.rules.push_back(r);
            }else{Profile profile;int nameOnly=0;std::string body;Config parsed;
                if(depth||c.profiles.size()>=8||values.size()!=6||!Unhex(k.substr(8),profile.name)||profile.name.size()>128||!Number(values[0],profile.trigger)||profile.trigger<0||profile.trigger>2||!Number(values[1],profile.start)||!Number(values[2],profile.end)||profile.start<0||profile.start>=1440||profile.end<0||profile.end>=1440||!Number(values[3],nameOnly)||nameOnly<0||nameOnly>1||(!values[4].empty()&&!Unhex(values[4],profile.app))||!Unhex(values[5],body,524288)||!Parse(body,parsed,error,depth+1)){error="Invalid profile";return false;}
                profile.nameOnly=nameOnly;if(profile.trigger==2&&(profile.app.empty()||(profile.nameOnly?profile.app.find_first_of("/\\:")!=std::string::npos:profile.app.size()<3||profile.app[1]!=':'||profile.app[2]!='\\'))){error="App profile needs an executable";return false;}profile.policy=parsed;c.profiles.push_back(std::move(profile));
            }continue;
        }
        if(k.starts_with("next.")) {
            modern=true;std::string id;if(!Unhex(k.substr(5),id)){error="Invalid next identity";return false;}id=Canonical(id);
            if(!keys.insert("canonical-next:"+id).second){error="Duplicate next identity";return false;}
            std::istringstream fields(v);std::string field;std::vector<int> nums;while(std::getline(fields,field,',')){int item;if(!Number(field,item)){error="Invalid stage value";return false;}nums.push_back(item);}
            if(nums.size()!=7||v.ends_with(',')||nums[3]<0||nums[3]>1){error="Invalid stage fields";return false;}
            auto& p=c.monitors[id];p.dim=nums[0];p.fadeMs=nums[1];p.dimSeconds=nums[2];p.batteryBlack=nums[3];p.sceneTheme=nums[4];p.sceneSeconds=nums[5];p.rotateSeconds=nums[6];continue;
        }
        if(k.starts_with("assets.")) {
            std::string id;if(!Unhex(k.substr(7),id)){error="Invalid asset identity";return false;}id=Canonical(id);
            if(!keys.insert("canonical-assets:"+id).second){error="Duplicate asset identity";return false;}
            std::istringstream fields(v);std::string field;std::vector<std::string> values;while(std::getline(fields,field,'|'))values.push_back(field);
            if(values.size()!=7||v.ends_with('|')){error="Invalid asset fields";return false;}
            auto& p=c.monitors[id];int nums[5]{};for(int i=0;i<5;++i)if(!Number(values[i+2],nums[i])){error="Invalid asset integer";return false;}
            if((!values[0].empty()&&!Unhex(values[0],p.custom))||(!values[1].empty()&&!Unhex(values[1],p.folder))||nums[2]<0||nums[2]>1||nums[3]<0||nums[3]>1){error="Invalid asset value";return false;}
            p.slideSeconds=nums[0];p.placement=nums[1];p.shuffle=nums[2];p.recursive=nums[3];p.background=nums[4];continue;
        }
        if(k.starts_with("policy.")) {
            std::string id;if(!Unhex(k.substr(7),id)) {error="Invalid policy identity";return false;}
            id=Canonical(id);std::istringstream fields(v);std::string field;std::vector<int> values;
            if(!keys.insert("canonical-policy:"+id).second){error="Duplicate policy identity";return false;}
            while(std::getline(fields,field,',')) {int item;if(!Number(field,item)){error="Invalid monitor policy";return false;}values.push_back(item);}
            if(values.size()!=7||v.ends_with(',')||values[1]<-1||values[1]>3||values[2]<-1||values[2]>1||values[4]<0||values[4]>1||values[6]<0||values[6]>1){error="Invalid monitor policy fields";return false;}
            auto& p=c.monitors[id];p.timeout=values[0];p.input=values[1];p.media=values[2];p.blackAfter=values[3];p.hardware=values[4]!=0;p.powerAfter=values[5];p.fullscreen=values[6]!=0;continue;
        }
        if(k.starts_with("monitor.")) {
            auto comma=v.find(','); std::string id; int en=0,saver=0;
            if(!Unhex(k.substr(8),id)||comma==std::string::npos||!Number(v.substr(0,comma),en)||en<0||en>1||!Number(v.substr(comma+1),saver)) { error="Invalid monitor preference"; return false; }
            id=Canonical(id); if(!keys.insert("canonical-monitor:"+id).second) {error="Duplicate monitor identity";return false;}
            c.monitors[id].enabled=en!=0;c.monitors[id].saver=saver; continue;
        }
        if(!Number(v,n)) { error="Expected integer"; return false; }
        if(k=="version") { if(n<1||n>3) { error="Unsupported version"; return false; } version=true;c.sourceSchema=n; }
        else if(k=="timeout") c.timeout=n; else if(k=="poll") c.poll=n; else if(k=="padding") c.padding=n;
        else if(k=="currentKey")c.currentKey=n;else if(k=="allKey")c.allKey=n;
        else {
            if(n<0||n>1) { error="Expected 0 or 1"; return false; }
            if(k=="automatic") c.automatic=n; else if(k=="perInput") c.perInput=n;
            else if(k=="media") c.media=n; else if(k=="perMedia") c.perMedia=n;
            else if(k=="muted") c.muted=n; else if(k=="debug") c.debug=n;
            else if(k=="hotkeys")c.hotkeys=n;else if(k=="controllerInput"){c.controllerInput=n;modern=true;}
            else { error="Unknown configuration key"; return false; }
        }
    }
    if(!version) { error="Missing version"; return false; }
    if(c.sourceSchema<3){if(modern){error="New fields require schema 3";return false;}for(auto& [id,p]:c.monitors){(void)id;if(p.input>1||p.saver>7){error="Invalid legacy mode";return false;}}}
    if(c.monitors.size()>64){error="Too many display preferences";return false;}
    if(!c.manualProfile.empty()&&std::none_of(c.profiles.begin(),c.profiles.end(),[&](const Profile& p){return p.name==c.manualProfile;})){error="Selected profile missing";return false;}
    Validate(c); result=std::move(c); return true;
}
template<class Store> bool Commit(Config& live,Config next,Store save) {
    Validate(next);auto text=Serialize(next);Config canonical;std::string error;
    if(!Parse(text,canonical,error)||!save(text))return false;live=std::move(canonical);return true;
}
struct Geometry { int64_t left,top,right,bottom; };
Geometry Padded(Geometry r,int padding) { int p=std::clamp(padding,0,1024); return {r.left-p,r.top-p,r.right+p,r.bottom+p}; }
bool MediaOverlap(Geometry window,Geometry display) {
    auto width=std::max<int64_t>(0,std::min(window.right,display.right)-std::max(window.left,display.left));
    auto height=std::max<int64_t>(0,std::min(window.bottom,display.bottom)-std::max(window.top,display.top));
    auto area=width*height,total=std::max<int64_t>(0,window.right-window.left)*std::max<int64_t>(0,window.bottom-window.top);
    return total>0&&area>=4096&&area*20>=total;
}
enum class State { Disabled,Desktop,Suppressed,Black,Launching,Saver,Fallback,Dim };
bool Running(State s) { return s==State::Dim||s==State::Black||s==State::Launching||s==State::Saver||s==State::Fallback; }
struct Node { Time last=0,generation=0; State state=State::Desktop; bool manual=false; Time began=0;bool sticky=false;int presentation=-2;bool powerRequested=false;Time stageBegan=0; };
struct InputObservation {std::set<std::string> targets;bool certain=false;};
InputObservation AttributeInput(bool keyboard,const std::string& cursor,const std::string& foreground,unsigned age) {
    InputObservation observation;if(!cursor.empty()) observation.targets.insert(cursor);
    if(keyboard&&!foreground.empty()) observation.targets.insert(foreground);
    observation.certain=!cursor.empty()&&(!keyboard||!foreground.empty())&&age<=250;return observation;
}
struct Media { Time at=0; bool valid=false,any=false,ambiguous=false; std::set<std::string> monitors; };
void AddAudioContribution(Media& media,const Policy& policy,const AppIdentity& identity,bool ambiguous,const std::set<std::string>& ids,bool owned=false) {
    if(owned)return;
    if(!ambiguous&&identity.known)for(auto& rule:policy.rules)if(rule.kind==1&&RuleMatch(rule,identity)&&(rule.global||!ids.empty()))return;
    media.any=true;media.monitors.insert(ids.begin(),ids.end());media.ambiguous=media.ambiguous||ambiguous||ids.empty()||!identity.known;
}
bool AudioBlocks(bool active,bool includeMuted,bool sessionMute,bool endpointMute,float peak,float volume,float endpointVolume) {
    return active && (includeMuted||(!sessionMute&&!endpointMute&&peak>0.0001f&&volume>0.0001f&&endpointVolume>0.0001f));
}
bool DisplayRequestBlocks(bool includeMuted,bool ownedSaverPresent,bool requested) { return includeMuted&&!ownedSaverPresent&&requested; }
struct MediaGrace {
    Media previous;
    Media Apply(Media current,int poll) {
        if(current.valid&&current.any) previous=current;
        else if(current.valid&&!current.any&&previous.any&&current.at>=previous.at&&current.at-previous.at<=static_cast<Time>(std::max(2000,poll*2))) {
            auto at=current.at; current=previous; current.at=at;
        } return current;
    }
};
// A responsive child is still only a structural health observation, never
// evidence of animation. Three consecutive one-second failures avoid reacting
// to one busy frame; startup has a separate five-second deadline.
struct PreviewHealth {
    Time began=0; bool ready=false; unsigned misses=0;
    int Sample(Time now,bool exists,bool responsive) {
        if(!ready&&now>=began&&now-began>=5000) return 2;
        if(exists&&responsive) {bool first=!ready;ready=true;misses=0;return first?1:0;}
        return ready&&++misses>=3?2:0;
    }
};
bool PowerDue(const Preference& p,State state,Time began,Time now) {
    return p.hardware&&p.powerAfter>0&&state!=State::Dim&&Running(state)&&now>=began&&now-began>=static_cast<Time>(p.powerAfter)*1000;
}
struct PowerDeadline {
    Time began=0,cancellation=0;bool recovering=false,off=false;
    int Sample(Time now,bool stop,bool acknowledged) {
        off=off||acknowledged;
        if(!recovering&&(stop||(!off&&now>=began&&now-began>=5000))){recovering=true;cancellation=now;return 1;}
        return recovering&&now>=cancellation&&now-cancellation>=5000?2:0;
    }
};
enum class Reason { Disabled,Session,Paused,Snoozed,ManualOnly,App,Fullscreen,Stale,Media,Idle,Dim,Manual,Sticky,Presentation,Fallback,HardwareFault };
enum class DeadlineKind { None,Activation,Presentation,Black,Hardware };
struct Explanation {Reason primary=Reason::Idle;uint32_t flags=0;DeadlineKind kind=DeadlineKind::None;Time remaining=0;};
enum class PowerSource {Unknown,AC,DC};
PowerSource PowerFromStatus(bool valid,unsigned status){return !valid?PowerSource::Unknown:status==0?PowerSource::DC:status==1?PowerSource::AC:PowerSource::Unknown;}
bool ClockMinute(const std::string& text,int& minute){int h=0,m=0;if(text.size()!=5||text[2]!=':'||!Number(text.substr(0,2),h)||!Number(text.substr(3),m)||h<0||h>23||m<0||m>59)return false;minute=h*60+m;return true;}
std::string ClockText(int minute){char text[6];snprintf(text,sizeof(text),"%02d:%02d",std::clamp(minute,0,1439)/60,std::clamp(minute,0,1439)%60);return text;}

struct Foreground {AppIdentity app;std::set<std::string> monitors;bool valid=true,owned=false;};
bool InSchedule(int minute,int start,int end){return start!=end&&(start<end?(minute>=start&&minute<end):(minute>=start||minute<end));}
std::string ResolveProfile(const Config& c,int minute,const AppIdentity& app) {
    if(!c.manualProfile.empty())return c.manualProfile;
    for(auto& p:c.profiles)if(p.trigger==2&&MatchApp(app,p.app,p.nameOnly))return p.name;
    for(auto& p:c.profiles)if(p.trigger==1&&InSchedule(minute,p.start,p.end))return p.name;
    return {};
}
Policy EffectivePolicy(const Config& c,const std::string& profile) {
    Policy result=c;
    for(auto& p:c.profiles)if(p.name==profile){result=p.policy;break;}
    // Saved base permissions cap profile eligibility. Profiles never grant DDC.
    for(auto& [id,p]:result.monitors){auto base=c.monitors.find(id);p.hardware=p.hardware&&base!=c.monitors.end()&&base->second.hardware;
        if(base!=c.monitors.end()&&!base->second.enabled)p.enabled=false;}
    for(auto& [id,p]:c.monitors)if(!result.monitors.contains(id))result.monitors[id]=p;
    return result;
}
std::string PolicyIdentity(const Policy& policy){Config c;static_cast<Policy&>(c)=policy;return Serialize(c);}
bool ControllerActive(unsigned buttons,int lx,int ly,int rx,int ry,unsigned lt,unsigned rt) {
    return (buttons&0xf3ff)||int64_t(lx)*lx+int64_t(ly)*ly>int64_t(7849)*7849||int64_t(rx)*rx+int64_t(ry)*ry>int64_t(8689)*8689||lt>30||rt>30;
}
unsigned DimAlpha(int percent,int fade,Time elapsed){return static_cast<unsigned>(std::clamp(percent,0,100)*255/100*(fade?std::min<Time>(elapsed,fade):1)/(fade?fade:1));}
// UI-owner policy accounting: fixed storage, no input values or identities.
#define DAC_WAKE_OBSERVABILITY 1
void SaturatingAdd(Time& value,Time amount=1){value=amount>UINT64_MAX-value?UINT64_MAX:value+amount;}
struct InputDiagnostics {Time desktopAccepted=0,activeAccepted=0,activeToDesktop=0;};
struct Controller {
    InputDiagnostics inputDiagnostics;
    Config config; std::map<std::string,Node> nodes; Time serial=0,inputBoundary=0; bool paused=false,blocked=false;
    Preference spanPreference;std::set<std::string> fullscreen,hardwareFaults,unidentified;bool legacyAutomationBlocked=false;
    Foreground foreground;PowerSource powerSource=PowerSource::Unknown;Time snoozeUntil=0;bool activityStale=false;
    std::string activeProfile,candidateProfile,effectiveIdentity,selectedManualProfile;Time candidateSince=0;
    Policy effective;bool effectiveReady=false;
    std::map<std::string,Explanation> explanations;std::map<std::string,Preference> manualPreferences;
    const Policy& PolicyNow()const{return effectiveReady?effective:static_cast<const Policy&>(config);}
    void Topology(const std::vector<std::string>& ids,Time now) {inputBoundary=now;nodes.clear();manualPreferences.clear();for(auto& id:ids)nodes[id]={now,++serial,State::Desktop,false};}
    Preference Pref(const std::string& id) const {
        if(id=="@span")return spanPreference;
        if(auto n=nodes.find(id);n!=nodes.end()&&n->second.manual&&Running(n->second.state)){auto m=manualPreferences.find(id);if(m!=manualPreferences.end()){auto p=m->second;auto base=config.monitors.find(id);p.hardware=p.hardware&&base!=config.monitors.end()&&base->second.hardware;if((base!=config.monitors.end()&&!base->second.enabled)||unidentified.contains(id))p.enabled=false;return p;}}
        auto& policy=PolicyNow();auto it=policy.monitors.find(id);auto p=it==policy.monitors.end()?Preference{}:it->second;auto base=config.monitors.find(id);p.hardware=p.hardware&&base!=config.monitors.end()&&base->second.hardware;if((base!=config.monitors.end()&&!base->second.enabled)||unidentified.contains(id))p.enabled=false;return p;
    }
    bool SelectPolicy(Time now,int minute) {
        bool manualChanged=config.manualProfile!=selectedManualProfile;selectedManualProfile=config.manualProfile;
        auto wanted=!config.manualProfile.empty()?config.manualProfile:foreground.owned&&!manualChanged?activeProfile:ResolveProfile(config,minute,foreground.valid?foreground.app:AppIdentity{});
        if(wanted!=candidateProfile){candidateProfile=wanted;candidateSince=now;}
        bool immediate=!effectiveReady||!config.manualProfile.empty()||manualChanged;
        if(!immediate&&(now<candidateSince||now-candidateSince<1000))return false;
        auto next=EffectivePolicy(config,wanted);if(legacyAutomationBlocked)next.automatic=false;auto identity=PolicyIdentity(next);
        if(effectiveReady&&identity==effectiveIdentity){activeProfile=wanted;return false;}
        effective=std::move(next);effectiveReady=true;effectiveIdentity=identity;activeProfile=wanted;
        for(auto& [id,n]:nodes){auto p=effective.monitors.find(id);bool enabled=p==effective.monitors.end()||p->second.enabled;
            if(!n.manual||!enabled)n={now,++serial,enabled?State::Desktop:State::Disabled,false};}
        return true;
    }
    void Reset(Time now) {inputBoundary=now;manualPreferences.clear();for(auto& [id,n]:nodes){(void)id;n={now,++serial,State::Desktop,false};}}
    void Snooze(Time now,int minutes){snoozeUntil=minutes>0?now+static_cast<Time>(minutes)*60000:0;for(auto& [id,n]:nodes){(void)id;if(!n.manual)n={now,++serial,State::Desktop,false};}}
    bool AcceptInput(const std::string& id,const Node& n,Time eventAt) const {
        return Pref(id).enabled&&!paused&&!blocked&&!n.sticky&&eventAt>inputBoundary&&eventAt>=n.last&&
            (!n.manual||eventAt>n.last)&&(!Running(n.state)||eventAt>=n.began);
    }
    void AccountAccepted(const Node& n){
        if(Running(n.state)){SaturatingAdd(inputDiagnostics.activeAccepted);SaturatingAdd(inputDiagnostics.activeToDesktop);}
        else if(n.state==State::Desktop)SaturatingAdd(inputDiagnostics.desktopAccepted);
    }
    void Input(Time now,const std::set<std::string>& targets,bool certain,Time eventAt=UINT64_MAX) {
        for(auto& [id,n]:nodes){auto p=Pref(id);bool local=p.input<0?PolicyNow().perInput:p.input!=0;
            if(AcceptInput(id,n,eventAt)&&(!local||(certain&&targets.contains(id))||id=="@span")){AccountAccepted(n);n={now,++serial,State::Desktop,false};}}
    }
    unsigned Activity(Time now,bool keyboard,const std::string& cursor,const std::set<std::string>& focus,bool certain,Time eventAt=UINT64_MAX,bool focusCertain=true) {
        unsigned accepted=0;
        for(auto& [id,n]:nodes){auto p=Pref(id);int mode=p.input<0?(PolicyNow().perInput?1:0):p.input;
            // The queued pointer position is historical. Foreground is only a fresh
            // heuristic: never credit today's focus for an old key or mouse event.
            bool pointer=certain&&cursor==id,focused=certain&&focusCertain&&focus.contains(id);
            bool hit=mode==0||(mode==2?pointer:mode==3?focused:pointer||(keyboard&&focused));
            if(AcceptInput(id,n,eventAt)&&(hit||id=="@span")){AccountAccepted(n);n={now,++serial,State::Desktop,false};++accepted;}}
        return accepted;
    }
    int Presentation(const Preference& p)const{return p.batteryBlack&&powerSource==PowerSource::DC?-1:p.saver;}
    void Manual(const std::string& id,Time now,bool sticky=false,int presentation=-2) {
        auto it=nodes.find(id);if(it==nodes.end()||!Pref(id).enabled||paused||blocked)return;
        auto p=Pref(id);manualPreferences[id]=p;int saver=presentation==-2?Presentation(p):presentation;
        it->second={now,++serial,saver<0?State::Black:State::Launching,true,now,sticky,saver};
    }
    Reason Inhibitor(const std::string& id,const Media& m,Time now,uint32_t& flags) const {
        auto& c=PolicyNow();auto p=Pref(id);Reason primary=Reason::Idle;auto add=[&](Reason r){flags|=1u<<static_cast<unsigned>(r);if(primary==Reason::Idle)primary=r;};
        if(c.controllerInput&&activityStale)add(Reason::Stale);
        for(auto& rule:c.rules)if(rule.kind==0&&!foreground.owned&&(!foreground.valid||!foreground.app.known||(RuleMatch(rule,foreground.app)&&(rule.global||foreground.monitors.empty()||foreground.monitors.contains(id)))))add(!foreground.valid||!foreground.app.known?Reason::Stale:Reason::App);
        if(p.fullscreen&&fullscreen.contains(id))add(Reason::Fullscreen);
        if(p.media<0?c.media:p.media!=0){if(!m.valid||now<m.at||now-m.at>static_cast<Time>(std::max(2000,c.poll*2)))add(Reason::Stale);
            else if(m.any&&(!c.perMedia||m.ambiguous||m.monitors.contains(id)))add(Reason::Media);}
        return primary;
    }
    bool Suppress(const std::string& id,const Media& m,Time now)const{uint32_t flags=0;return Inhibitor(id,m,now,flags)!=Reason::Idle;}
    Explanation Explain(const std::string& id,const Media& m,Time now)const {
        Explanation e;auto it=nodes.find(id);if(it==nodes.end()){e.primary=Reason::Disabled;e.flags=1u<<static_cast<unsigned>(e.primary);return e;}auto& n=it->second;auto p=Pref(id);
        if(hardwareFaults.contains(id))e.flags|=1u<<static_cast<unsigned>(Reason::HardwareFault);
        if(!p.enabled)e.primary=Reason::Disabled;else if(blocked)e.primary=Reason::Session;else if(paused)e.primary=Reason::Paused;
        else if(id!="@span"&&nodes.contains("@span")&&Running(nodes.at("@span").state))e.primary=Reason::Presentation;
        else if(Running(n.state)){
            e.primary=n.state==State::Fallback?Reason::Fallback:n.state==State::Dim?Reason::Dim:n.sticky?Reason::Sticky:n.manual?Reason::Manual:Reason::Presentation;
            auto deadline=[&](Time origin,int seconds,DeadlineKind kind){if(seconds<=0||now<origin)return;Time until=origin+static_cast<Time>(seconds)*1000,remaining=now>=until?0:until-now;if(e.kind==DeadlineKind::None||remaining<e.remaining){e.kind=kind;e.remaining=remaining;}};
            if(n.state==State::Dim)deadline(n.stageBegan,p.dimSeconds,DeadlineKind::Presentation);
            else {if(n.state!=State::Black){deadline(n.began,p.blackAfter,DeadlineKind::Black);if(n.presentation>=8)deadline(n.began,p.sceneSeconds,DeadlineKind::Black);if(p.hardware&&hardwareFaults.contains(id))deadline(n.began,p.powerAfter,DeadlineKind::Black);}if(p.hardware&&!hardwareFaults.contains(id)&&!n.powerRequested)deadline(n.began,p.powerAfter,DeadlineKind::Hardware);}
        }else if(snoozeUntil>now){e.primary=Reason::Snoozed;e.remaining=snoozeUntil-now;}
        else if(!PolicyNow().automatic||legacyAutomationBlocked)e.primary=Reason::ManualOnly;
        else {e.primary=Inhibitor(id,m,now,e.flags);if(e.primary==Reason::Idle&&now>=n.last){e.kind=DeadlineKind::Activation;Time delay=static_cast<Time>(p.timeout?p.timeout:PolicyNow().timeout)*1000;e.remaining=now-n.last>=delay?0:delay-(now-n.last);}}
        e.flags|=1u<<static_cast<unsigned>(e.primary);return e;
    }
    void Tick(Time now,const Media& m) {
        if(snoozeUntil&&now>=snoozeUntil)Snooze(now,0);
        bool spanning=nodes.contains("@span")&&Running(nodes.at("@span").state);
        for(auto& [id,n]:nodes){auto p=Pref(id);auto& c=PolicyNow();
            if(spanning&&id!="@span"){n.last=now;n.state=State::Desktop;explanations[id]=Explain(id,m,now);continue;}
            if(!p.enabled||paused||blocked){if(Running(n.state))n.generation=++serial;n.state=p.enabled?State::Desktop:State::Disabled;n.manual=false;n.last=now;explanations[id]=Explain(id,m,now);continue;}
            if(Running(n.state)&&n.state!=State::Dim&&n.state!=State::Black&&now>=n.began&&((p.blackAfter>0&&now-n.began>=static_cast<Time>(p.blackAfter)*1000)||(p.hardware&&p.powerAfter>0&&now-n.began>=static_cast<Time>(p.powerAfter)*1000)||(n.presentation>=8&&now-n.began>=static_cast<Time>(p.sceneSeconds)*1000)||(p.batteryBlack&&powerSource==PowerSource::DC))){n.state=State::Black;n.presentation=-1;n.generation=++serial;}
            if(!n.manual){
                if(!c.automatic||legacyAutomationBlocked||snoozeUntil>now){if(Running(n.state))n.generation=++serial;n.state=State::Desktop;n.last=now;}
                else if(Suppress(id,m,now)){if(Running(n.state))n.generation=++serial;n.state=State::Suppressed;n.last=now;}
                else if(now<n.last)n.last=now;
                else if(n.state==State::Dim&&now>=n.stageBegan&&now-n.stageBegan>=static_cast<Time>(p.dimSeconds)*1000){n.generation=++serial;n.presentation=Presentation(p);n.state=n.presentation<0?State::Black:State::Launching;n.began=now;}
                else if(now-n.last>=static_cast<Time>(p.timeout?p.timeout:c.timeout)*1000&&!Running(n.state)){n.generation=++serial;n.presentation=Presentation(p);n.state=p.dim>0?State::Dim:n.presentation<0?State::Black:State::Launching;n.began=now;n.stageBegan=now;}
                else if(n.state==State::Suppressed)n.state=State::Desktop;
            }explanations[id]=Explain(id,m,now);
        }
    }
    bool Result(const std::string& id,Time generation,bool healthy){auto it=nodes.find(id);if(it==nodes.end()||it->second.generation!=generation||!Running(it->second.state)||it->second.state==State::Black||it->second.state==State::Dim)return false;if(healthy&&it->second.state==State::Fallback)return false;it->second.state=healthy?State::Saver:State::Fallback;return true;}
    bool Any()const{for(auto& [id,n]:nodes){(void)id;if(Running(n.state))return true;}return false;}
};
// Deliberate importer: parse into a copy, preserve the source, retain unknown
// display preferences. Numeric monitor aliases resolve only against this snapshot.
bool Import(const std::string& text,const std::vector<std::pair<std::string,std::string>>& displays,
            Config& c,int& ignored,std::string& error) {
    if(text.size()>262144) { error="Legacy file exceeds 256 KiB"; return false; }
    Config next=c; std::istringstream in(text); std::string line; int recognized=0; ignored=0;
    while(std::getline(in,line)) {
        line=Trim(line.substr(0,line.find(';'))); if(line.empty()||line[0]=='#'||line[0]=='[') continue;
        auto e=line.find('='); if(e==std::string::npos) { error="Malformed legacy line"; return false; }
        auto k=Trim(line.substr(0,e)),v=Trim(line.substr(e+1)); int n;
        bool known=k=="idleTimeout"||k=="checkInterval"||k=="pixelShiftCompensation"||k=="startupEnabled"||k=="debugMode"||k=="perMonitorInputDetection"||k=="perMonitorMediaDetection"||k=="blockOnMutedMedia"||k=="mediaDetectionEnabled"||k=="audioDetectionEnabled"||k.starts_with("monitor");
        if(!known) { ++ignored; continue; }
        if(!Number(v,n)) { error="Invalid legacy integer"; return false; }
        if(k=="idleTimeout") next.timeout=n; else if(k=="checkInterval") next.poll=n;
        else if(k=="pixelShiftCompensation") next.padding=n; else if(k=="startupEnabled") next.automatic=n!=0;
        else if(k=="debugMode") next.debug=n!=0; else if(k=="perMonitorInputDetection") next.perInput=n!=0;
        else if(k=="perMonitorMediaDetection") next.perMedia=n!=0; else if(k=="blockOnMutedMedia") next.muted=n!=0;
        else if(k=="mediaDetectionEnabled"||k=="audioDetectionEnabled") next.media=n!=0;
        else {
            std::string id; int index;
            if(k.starts_with("monitorEnabled_")) {
                id=Canonical(k.substr(15)); for(auto& [stable,alias]:displays) if(id==Canonical(alias)||id==Canonical(stable)) { id=Canonical(stable); break; }
            } else if(Number(k.substr(7),index)&&index>=0&&index<static_cast<int>(displays.size())) id=displays[index].first;
            else { ++ignored; continue; }
            if(id.empty()) { error="Empty legacy monitor identity"; return false; }
            next.monitors[id].enabled=n!=0;
        }
        ++recognized;
    }
    if(!recognized) { error="No recognized legacy settings"; return false; }
    // Import carries preferences, never authority to start presentations or
    // power hardware. Require an explicit local decision after reviewing it.
    DisarmImportedConfig(next);
    Validate(next); c=std::move(next); return true;
}
} // namespace dac
