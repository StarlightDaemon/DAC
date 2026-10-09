#define DAC_POLICY_ONLY
#include "../src/dac.cpp"
#include <cstdlib>
using namespace dac;
int checks=0;
void Check(bool ok,const char* what) { ++checks; if(!ok) { fprintf(stderr,"FAIL %s\n",what); std::exit(1); } }

void NextPolicy() {
    std::string error;Config c,copy;auto original=Serialize(copy);
    for(auto text:{"# comment\ntimeout=17\nversion=2\nperInput=1\nmonitor.41=0,1\n","version=2\npolicy.41=0,1,-1,5,0,0,0\n"}){Check(Parse(text,c,error)&&c.sourceSchema==2&&c.monitors["A"].dim==0,"schema two migration retains disabled/local/stage defaults");}
    for(auto bad:{"ff","c080","e08080","f0808080","eda080","f4908080","e282","00"}){Check(!Parse(std::string("version=3\nmonitor.")+bad+"=0,1\n",copy,error),"decoded invalid UTF-8 rejected");Check(Serialize(copy)==original,"failed parse preserves prior output");}
    Check(ValidUtf8("\xf0\x9f\x8c\x99")&&ValidUtf8("\xce\xbb"),"valid astral/BMP UTF-8");
    for(auto bad:{"version=4\n","version=2\nnext.41=1,1,1,0,0,5,0\n","version=2\npolicy.41=0,2,0,0,0,0,0\n","version=3\nmanualProfile=41\n","version=3\nrule.0=0|0|0|433a5c612e657865|\n"})Check(!Parse(bad,copy,error),"forward/malformed/dangling schema rejection");
    c=Config{};Profile profile;profile.name="Work";profile.policy.timeout=1;profile.policy.monitors["A"].dim=999;c.profiles.push_back(profile);
    Check(Commit(copy,c,[](auto&){return true;})&&copy.profiles[0].policy.timeout==5&&copy.profiles[0].policy.monitors["A"].dim==100,"nested profile canonicalized in live commit");
    c.profiles.resize(9);Check(!Commit(copy,c,[](auto&){return true;}),"profile count/duplicate bound rejects before store");
    c=Config{};for(int i=0;i<65;++i)c.monitors[std::to_string(i)]={};Check(!Commit(copy,c,[](auto&){return true;}),"display bound rejects persisted unreadable configuration");
    c=Config{};c.rules.resize(33,{0,false,false,"C:\\a.exe"});Check(!Commit(copy,c,[](auto&){return true;}),"rule bound");
    AppIdentity app{"c:\\apps\\game.exe","game.exe",true},other{"c:\\other\\game.exe","game.exe",true};AppRule rule{0,false,false,"C:\\Apps\\Game.exe"};
    Check(RuleMatch(rule,app)&&!RuleMatch(rule,other)&&!RuleMatch(rule,{}),"full executable path with case folding and unknown identity");rule.nameOnly=true;rule.executable="game.exe";Check(RuleMatch(rule,other),"explicit weaker filename match");
    Policy audio;audio.rules={{1,false,false,app.path}};Media m{1,true,false,false,{}};AddAudioContribution(m,audio,app,false,{"A"});Check(!m.any,"known source display audio ignore");AddAudioContribution(m,audio,other,false,{"A"});Check(m.any&&m.monitors.contains("A"),"ignored source never erases nonignored player");
    m={1,true,false,false,{}};AddAudioContribution(m,audio,app,true,{"A"});Check(m.any&&m.ambiguous,"multi-process browser source cannot be ignored");m={1,true,false,false,{}};AddAudioContribution(m,audio,{},false,{"A"});Check(m.any&&m.ambiguous,"denied/exited process stays conservative");m={1,true,false,false,{}};AddAudioContribution(m,audio,app,false,{});Check(m.any&&m.ambiguous,"display rule cannot discard unattributed source");audio.rules[0].global=true;m={1,true,false,false,{}};AddAudioContribution(m,audio,app,false,{});Check(!m.any,"global exact-source ignore includes known unattributed source");AddAudioContribution(m,audio,other,false,{"B"},true);Check(!m.any,"owned playback excluded");
    Controller p;p.config.media=false;p.config.automatic=true;p.config.timeout=5;p.Topology({"A","B","C"},0);
    for(int mode=0;mode<=3;++mode){p.Reset(0);for(auto id:{"A","B","C"})p.config.monitors[id].input=mode;p.Activity(100,true,"A",{"B","C"},true);Check((p.nodes["A"].last==100)==(mode!=3),"keyboard pointer mode attribution");Check((p.nodes["B"].last==100)==(mode!=2),"keyboard foreground overlap attribution");auto before=p.nodes;p.Activity(200,true,"",{},true);Check(mode==0?(p.nodes["A"].last==200&&p.nodes["B"].last==200):(p.nodes["A"].last==before["A"].last&&p.nodes["B"].last==before["B"].last),"missing attribution only credits explicit shared mode");}
    p.config.monitors.clear();p.Reset(0);Check(p.Explain("A",{},4999).kind==DeadlineKind::Activation&&p.Explain("A",{},4999).remaining==1,"idle explanation exact boundary");p.unidentified.insert("A");Check(p.Explain("A",{},5000).primary==Reason::Disabled&&p.Explain("A",{},5000).kind==DeadlineKind::None,"unidentified has no misleading countdown");p.unidentified.clear();
    p.config.rules={{0,false,false,app.path}};p.foreground={app,{"A"},true};p.fullscreen={"A"};p.config.monitors["A"].fullscreen=true;p.config.monitors["A"].media=1;auto e=p.Explain("A",{},5000);Check(e.primary==Reason::App&&(e.flags&(1u<<unsigned(Reason::Fullscreen)))&&(e.flags&(1u<<unsigned(Reason::Stale)))&&e.kind==DeadlineKind::None,"coexisting inhibition flags and deterministic primary");
    p.foreground.app={};Check(p.Explain("A",{},5000).primary==Reason::Stale,"unknown process is not a guessed exception");p.foreground.owned=true;p.config.monitors["A"].fullscreen=false;p.config.monitors["A"].media=0;Check(p.Explain("A",{},5000).primary==Reason::Idle,"owned presentation never self-inhibits app rule");
    p.config.rules.clear();p.foreground={};p.config.monitors.clear();p.Reset(0);p.Tick(5000,{});p.Manual("B",5000,true,8);auto manual=p.nodes["B"];p.Snooze(6000,5);Check(!Running(p.nodes["A"].state)&&p.nodes["B"].generation==manual.generation&&p.nodes["B"].began==manual.began,"snooze cancels automatic only");Check(p.Explain("A",{},6000).primary==Reason::Snoozed,"snooze reason");p.Manual("C",6001);Check(p.nodes["C"].manual,"manual remains available during snooze");p.Tick(305999,{});Check(!Running(p.nodes["A"].state),"before snooze expiry");p.Tick(306000,{});Check(!p.snoozeUntil&&p.nodes["A"].last==306000&&!Running(p.nodes["A"].state),"expiry starts fresh idle");p.Tick(310999,{});Check(!Running(p.nodes["A"].state),"fresh interval before boundary");p.Tick(311000,{});Check(Running(p.nodes["A"].state),"fresh idle boundary after snooze");
    p.paused=true;p.Tick(311001,{});Check(p.Explain("A",{},311001).primary==Reason::Paused&&!p.Any(),"pause distinct and authoritative");p.blocked=true;Check(p.Explain("A",{},311001).primary==Reason::Session,"session precedes pause");p.config.monitors["A"].enabled=false;Check(p.Explain("A",{},311001).primary==Reason::Disabled,"disabled precedes all");
    Controller stage;stage.config.media=false;stage.config.automatic=true;stage.config.timeout=5;auto& pref=stage.config.monitors["A"];pref.dim=50;pref.fadeMs=1000;pref.dimSeconds=3;pref.saver=8;pref.blackAfter=5;pref.hardware=true;pref.powerAfter=8;stage.Topology({"A"},0);stage.Tick(5000,{});Check(stage.nodes["A"].state==State::Dim&&!PowerDue(pref,State::Dim,0,999999),"dim is not presentation or hardware origin");stage.Tick(7999,{});Check(stage.Explain("A",{},7999).remaining==1,"dim transition boundary countdown");stage.Tick(8000,{});Check(stage.nodes["A"].began==8000&&stage.nodes["A"].state==State::Launching,"presentation origin follows warning");auto generation=stage.nodes["A"].generation;stage.Tick(12999,{});Check(stage.nodes["A"].state!=State::Black,"legacy black delay not shortened by dim");stage.Tick(13000,{});Check(stage.nodes["A"].state==State::Black&&!stage.Result("A",generation,true),"black boundary and late result rejection");
    Check(DimAlpha(0,1000,500)==0&&DimAlpha(100,0,0)==255&&DimAlpha(100,1000,500)==127&&DimAlpha(100,1000,1000)==255,"opacity fade zero/half/full boundaries");
    Check(!ControllerActive(0,7849,0,8689,0,30,30)&&ControllerActive(0,7850,0,0,0,0,0)&&ControllerActive(0,-32768,-32768,0,0,0,0)&&ControllerActive(1,0,0,0,0,0,0)&&!ControllerActive(0x400,0,0,0,0,0,0),"controller radial deadzones, signed extremes, held buttons and reserved bits");
    Check(PowerFromStatus(true,0)==PowerSource::DC&&PowerFromStatus(true,1)==PowerSource::AC&&PowerFromStatus(true,255)==PowerSource::Unknown&&PowerFromStatus(false,0)==PowerSource::Unknown,"AC DC unknown and API failure");pref.batteryBlack=true;stage.powerSource=PowerSource::DC;Check(stage.Presentation(pref)==-1&&pref.saver==8,"battery substitutes without overwriting assignment");stage.powerSource=PowerSource::Unknown;Check(stage.Presentation(pref)==8,"unknown retains saved assignment");
    Config profiles;profiles.media=false;profiles.automatic=true;profiles.timeout=5;profiles.monitors["A"].hardware=true;profiles.monitors["disabled"].enabled=false;Profile night;night.name="Night";night.trigger=1;night.start=1380;night.end=60;night.policy=profiles;night.policy.monitors["A"].hardware=false;night.policy.monitors["A"].powerAfter=5;night.policy.monitors["disabled"].enabled=true;Profile gaming=night;gaming.name="Gaming";gaming.trigger=2;gaming.app=app.path;profiles.profiles={night,gaming};
    Check(ResolveProfile(profiles,1380,{})=="Night"&&ResolveProfile(profiles,59,{})=="Night"&&ResolveProfile(profiles,60,{}).empty()&&ResolveProfile(profiles,1379,{}).empty()&&!InSchedule(500,500,500),"overnight/end-exclusive/equal schedule boundaries");Check(ResolveProfile(profiles,0,app)=="Gaming","app precedes schedule");profiles.manualProfile="Night";Check(ResolveProfile(profiles,0,app)=="Night","manual selection precedes triggers");profiles.manualProfile.clear();auto effective=EffectivePolicy(profiles,"Night");Check(!effective.monitors["A"].hardware&&!effective.monitors["disabled"].enabled,"profile cannot widen disabled or hardware grants");
    Controller selected;selected.config=profiles;selected.Topology({"A","disabled"},0);selected.SelectPolicy(0,100);selected.foreground={app,{"A"},true};selected.SelectPolicy(100,100);selected.foreground.app={};selected.SelectPolicy(600,100);selected.foreground.app=app;selected.SelectPolicy(900,100);Check(selected.activeProfile.empty(),"rapid app switching debounced");Check(selected.SelectPolicy(1900,100)&&selected.activeProfile=="Gaming","stable app profile applied once");auto serial=selected.serial;for(Time t=1901;t<4000;t+=50)Check(!selected.SelectPolicy(t,100)&&selected.serial==serial,"unchanged effective policy never resets every poll");selected.foreground.valid=false;selected.SelectPolicy(4100,100);selected.SelectPolicy(5100,100);Check(selected.activeProfile.empty(),"stale app identity cannot select profile");selected.config.monitors["A"].hardware=true;selected.Manual("A",5200,true);selected.config.monitors["A"].hardware=false;Check(!selected.Pref("A").hardware,"manual captured permission capped after revocation");selected.legacyAutomationBlocked=true;selected.Reset(0);selected.Tick(9000,{});Check(!selected.Any(),"legacy startup caps profile/base automation");
    Check(ExecutableEqual("C:\\\xc3\x89\\Game.exe","c:\\\xc3\xa9\\game.exe"),"Unicode Windows executable case matching");
    Controller owned;owned.config=profiles;owned.Topology({"A"},0);owned.config.manualProfile="Night";owned.SelectPolicy(1,100);owned.Manual("A",2,true);auto kept=owned.nodes["A"];owned.foreground.owned=true;owned.config.manualProfile="Gaming";owned.SelectPolicy(3,100);Check(owned.activeProfile=="Gaming"&&owned.nodes["A"].generation==kept.generation&&owned.nodes["A"].began==kept.began,"explicit manual selection precedes owned focus and preserves manual session");owned.config.manualProfile.clear();owned.SelectPolicy(4,100);Check(owned.activeProfile.empty(),"explicit automatic release resolves while settings owns focus");
    int minute=0;Check(ClockMinute("23:59",minute)&&minute==1439&&ClockText(0)=="00:00"&&!ClockMinute("24:00",minute)&&!ClockMinute("09:60",minute),"plain-language HH:MM schedule form");
    puts("POLICY next-beta: schema3/migration, UTF-8, reasons, app sources, input modes, snooze, profiles, permission caps, stages, XInput and battery pass");
}

void IndependentDisplayInput(){
    const std::vector<std::string> ids={"A","B","C"};
    Controller fallback;fallback.Topology(ids,0);for(auto& id:ids)fallback.Manual(id,10);
    auto bGeneration=fallback.nodes["B"].generation,cGeneration=fallback.nodes["C"].generation;
    // The timer can observe LASTINPUTINFO before the corresponding WM_INPUT.
    // A missing/raw-delayed event must never become activity on every display.
    fallback.Input(20,{},false);
    fallback.Activity(21,false,"A",{"B"},true);
    fallback.Input(22,{},false);
    fallback.Activity(23,false,"A",{"B"},false);
    Check(fallback.nodes["A"].state==State::Desktop&&Running(fallback.nodes["B"].state)&&Running(fallback.nodes["C"].state)&&fallback.nodes["B"].generation==bGeneration&&fallback.nodes["C"].generation==cGeneration,"unattributed fallback before/after primary mouse input preserves other idle displays");
    fallback.Activity(24,true,"A",{},true);
    Check(Running(fallback.nodes["B"].state)&&Running(fallback.nodes["C"].state),"missing keyboard focus never expands to every display");
    fallback.config.monitors["B"].input=0;fallback.nodes["B"].manual=false;fallback.Input(25,{},false);
    Check(!Running(fallback.nodes["B"].state)&&Running(fallback.nodes["C"].state),"explicit shared input still accepts unattributed activity");
    for(auto& first:ids)for(auto& second:ids){
        if(first==second)continue;
        Controller local;Check(local.config.perInput,"fresh policy defaults to independent display input");
        local.Topology(ids,0);for(auto& id:ids)local.Manual(id,10);
        std::string third;for(auto& id:ids)if(id!=first&&id!=second)third=id;
        auto generation=local.nodes[third].generation;
        // A focused window on the protected third display must not make mouse
        // activity on either of the other two wake it.
        local.Activity(20,false,first,{third},true);
        Check(local.nodes[first].state==State::Desktop&&Running(local.nodes[second].state)&&Running(local.nodes[third].state),"mouse wakes only its display in every three-monitor ordering");
        local.Activity(21,false,second,{third},true);
        Check(local.nodes[first].state==State::Desktop&&local.nodes[second].state==State::Desktop&&Running(local.nodes[third].state)&&local.nodes[third].generation==generation,"using any ordered pair preserves the third presentation and generation");
    }
    Controller keyboard;keyboard.Topology(ids,0);for(auto& id:ids)keyboard.Manual(id,10);
    keyboard.Activity(20,true,"A",{"B"},true);
    Check(keyboard.nodes["A"].state==State::Desktop&&keyboard.nodes["B"].state==State::Desktop&&Running(keyboard.nodes["C"].state),"keyboard credits pointer and focus without waking an unrelated third display");
}

#ifdef DAC_WAKE_OBSERVABILITY
void WakeAccountingPolicy(){
    for(int count:{1,2,3})for(unsigned mask=0;mask<(1u<<count);++mask)for(bool automatic:{false,true})for(int style:{-1,8,9})for(int target=0;target<count;++target){
        Controller p;p.config.media=false;p.config.automatic=automatic;p.config.timeout=5;std::vector<std::string> ids;
        for(int i=0;i<count;++i){ids.push_back(std::to_string(i));p.config.monitors[ids.back()].saver=style;}
        p.Topology(ids,1);
        for(int i=0;i<count;++i){if(automatic)p.nodes[ids[i]].last=(mask&(1u<<i))?1:6001;else if(mask&(1u<<i))p.Manual(ids[i],6001,false,style);}
        if(automatic)p.Tick(6001,{});
        for(auto& [id,n]:p.nodes)if(n.state==State::Launching)Check(p.Result(id,n.generation,true),"coverage scenes reach actual Saver state");
        auto before=p.nodes;auto id=ids[target];bool active=!!(mask&(1u<<target));
        Check(p.Activity(7001,false,id,{},true,6501,false)==1,"partial/full coverage accepts exactly one independent target");
        Check(p.inputDiagnostics.desktopAccepted==Time(!active)&&p.inputDiagnostics.activeAccepted==Time(active)&&p.inputDiagnostics.activeToDesktop==Time(active),"policy accounting separates desktop credit and active transitions");
        for(auto& peer:ids)if(peer!=id){auto& a=p.nodes.at(peer);auto& b=before.at(peer);Check(a.generation==b.generation&&a.last==b.last&&a.began==b.began&&a.state==b.state&&a.manual==b.manual&&a.sticky==b.sticky&&a.presentation==b.presentation&&a.stageBegan==b.stageBegan&&a.powerRequested==b.powerRequested,"all unaffected policy session fields preserved");}
        p.Activity(7101,false,id,{},true,7002,false);Check(p.inputDiagnostics.desktopAccepted==Time(!active)+1&&p.inputDiagnostics.activeToDesktop==Time(active),"desktop repeat is not another wake transition");
    }
    for(bool automatic:{false,true}){
        Controller p;p.config.media=false;p.config.automatic=automatic;p.config.timeout=5;p.config.monitors["A"].saver=9;p.Topology({"A"},1);
        if(automatic)p.Tick(6001,{});else p.Manual("A",6001,false,9);Check(p.Result("A",p.nodes["A"].generation,true),"activation-boundary scene ready");auto original=p.nodes["A"];
        Check(!p.Activity(7001,false,"A",{},true,6000,false)&&p.nodes["A"].generation==original.generation&&!p.inputDiagnostics.activeToDesktop,"pre-activation input rejects without diagnostic transition");
        Check(p.Activity(7002,false,"A",{},true,6002,false)==1&&p.inputDiagnostics.activeToDesktop==1,"post-activation input transitions once");
    }
    Controller fallback;fallback.Topology({"A","B"},1);fallback.config.perInput=false;fallback.Manual("A",2);fallback.Input(3,{},false,3);
    Check(fallback.inputDiagnostics.activeToDesktop==1&&fallback.inputDiagnostics.desktopAccepted==1,"fallback shared accounting includes actual active and desktop nodes");
    Time saturated=UINT64_MAX-1;SaturatingAdd(saturated,5);SaturatingAdd(saturated);Check(saturated==UINT64_MAX,"policy counter never wraps");
    fallback.inputDiagnostics={UINT64_MAX,UINT64_MAX,UINT64_MAX};fallback.Manual("A",4);fallback.Input(5,{},false,5);
    Check(fallback.inputDiagnostics.desktopAccepted==UINT64_MAX&&fallback.inputDiagnostics.activeAccepted==UINT64_MAX&&fallback.inputDiagnostics.activeToDesktop==UINT64_MAX,"accepted policy counters saturate under real dispatch");
    puts("POLICY wake accounting PASS: partial/full/empty coverage, manual/automatic steady states, unchanged peers, boundaries and saturation");
}

#endif // unchanged-source performance comparison
void StandalonePreferences() {
    Config original;original.automatic=true;original.monitors["A"].hardware=true;original.monitors["A"].powerAfter=10;
    Profile profile;profile.name="Imported schedule";profile.trigger=1;profile.start=60;profile.end=120;profile.policy=original;
    original.profiles.push_back(profile);original.rules.push_back({0,false,false,"C:\\example.exe"});
    Config full;std::string error;
    Check(Parse(Serialize(original),full,error)&&Serialize(full)==Serialize(original),"whole protection settings export/import preserves profiles rules and monitors");
    DisarmImportedConfig(full);
    Check(!full.automatic&&!full.monitors["A"].hardware&&!full.monitors["A"].powerAfter&&!full.profiles[0].policy.automatic&&!full.profiles[0].policy.monitors["A"].hardware,"full settings import never transfers activation authority");
    Check(full.rules.size()==1&&full.profiles[0].start==60&&full.profiles[0].end==120,"disarming retains imported rules and schedules");
    for(bool setup:{false,true})for(int action=0;action<3;++action)for(int appearance=0;appearance<3;++appearance){
        IntegrationSettings input{setup,action,appearance},output;
        Check(ParseIntegration(SerializeIntegration(input),output),"standalone host settings roundtrip");
        Check(output.startupSetup==setup&&output.trayAction==action&&output.appearance==appearance,"standalone host settings preserve values");
    }
    IntegrationSettings retained{true,2,2};
    for(const auto& bad:std::vector<std::string>{"", "version=2\nstartupSetup=0\ntrayAction=0\nappearance=0\n", "version=1\nstartupSetup=0\ntrayAction=0\nappearance=3\n", "version=1\nstartupSetup=0\ntrayAction=0\nappearance=0\nversion=1\n", "version=1\nstartupSetup=0\ntrayAction=0\n",std::string(1025,'a')}){
        Check(!ParseIntegration(bad,retained),"invalid host settings rejected");
        Check(retained.startupSetup&&retained.trayAction==2&&retained.appearance==2,"host parse failure preserves caller state");
    }
}
int main() {
    StandalonePreferences();
#ifdef DAC_WAKE_OBSERVABILITY
    WakeAccountingPolicy();
#endif
    IndependentDisplayInput();NextPolicy();
    PreviewHealth health{100};Check(health.Sample(101,true,true)==1,"owned responsive preview first observation");
    Check(health.Sample(1100,true,false)==0&&health.Sample(2100,false,false)==0&&health.Sample(3100,true,true)==0,"busy-frame/disappeared-child recovery before threshold");
    Check(health.Sample(4100,true,false)==0&&health.Sample(5100,true,false)==0&&health.Sample(6100,true,false)==2,"three post-start health failures become fallback");
    PreviewHealth startup{100};Check(startup.Sample(5099,false,false)==0&&startup.Sample(5100,true,true)==2,"startup deadline not extended by late child");
    Config c; c.monitors={{"display-A",{false,1}},{"display-B",{false,2}},{"disconnected-\xc3\xa9",{true,5}}};
    std::string error; Config copy;
    Check(Parse(Serialize(c),copy,error)&&Serialize(c)==Serialize(copy),"all-disabled / disconnected Unicode roundtrip");
    auto prior=Serialize(copy);
    Check(!Commit(copy,Config{},[](const std::string&){return false;})&&Serialize(copy)==prior,"failed save retains live configuration");
    Check(Commit(copy,c,[](const std::string&){return true;}),"successful save");
    for(auto text:{"version=4\n","version=1\npoll=abc\n","version=1\npoll=2\npoll=3\n","version=1\nmonitor.zz=0,1\n","version=1\nautomatic=2\n","version=1\nunknown=1\n","timeout=5\n"})
        Check(!Parse(text,copy,error),"malformed/version config rejection");
    Config rich;auto& pref=rich.monitors["A"];pref.timeout=10;pref.input=1;pref.media=0;pref.blackAfter=7;pref.hardware=true;pref.powerAfter=12;pref.fullscreen=true;
    pref.saver=7;pref.folder="C:\\photos-\xce\xbb";pref.custom="C:\\trusted.scr";pref.slideSeconds=21;pref.placement=4;pref.shuffle=pref.recursive=true;pref.background=0x112233;rich.hotkeys=true;rich.currentKey=3;rich.allKey=4;
    Check(Parse(Serialize(rich),copy,error)&&Serialize(copy)==Serialize(rich),"v2 per-display controls, assets and hotkeys roundtrip");
    Check(Parse("version=1\nmonitor.41=0,5\n",copy,error)&&!copy.monitors["A"].hardware&&copy.monitors["A"].timeout==0,"v1 migration never opts into power");
    for(auto text:{"version=2\npolicy.41=0,2,0,0,0,0,0\n","version=2\npolicy.41=0,0,0,0,1,0,0,\n","version=2\nassets.41=||30|0|2|0|0\n","version=2\nassets.41=zz||30|0|0|0|0\n"})Check(!Parse(text,copy,error),"malformed advanced preferences rejected");
    Check(Parse("version=2\npolicy.41=0,1,-1,5,0,0,1\nmonitor.41=0,6\n",copy,error)&&!copy.monitors["A"].enabled&&copy.monitors["A"].input==1,"policy ordering independent of assignment");
    Check(AttributeInput(false,"A","B",200).targets==std::set<std::string>{"A"},"mouse credits cursor only");
    Check(AttributeInput(true,"A","B",250).targets==std::set<std::string>({"A","B"})&&AttributeInput(true,"A","B",250).certain,"keyboard credits both observed monitors");
    Check(!AttributeInput(true,"A","",1).certain&&!AttributeInput(false,"A","B",251).certain,"missing or delayed observations become conservative");
    Controller advanced;advanced.config.media=false;advanced.config.automatic=true;advanced.config.timeout=5;advanced.config.monitors["A"].timeout=10;advanced.Topology({"A","B"},0);
    advanced.Tick(5000,{});Check(!Running(advanced.nodes["A"].state)&&Running(advanced.nodes["B"].state),"independent idle deadlines");
    advanced.Tick(10000,{});Check(advanced.Any(),"individual idle boundary");advanced.Reset(0);advanced.config.monitors["A"].media=1;advanced.Tick(10000,{});
    Check(advanced.nodes["A"].state==State::Suppressed&&Running(advanced.nodes["B"].state),"media override on even when global off");
    advanced.config.monitors["A"].media=0;advanced.config.monitors["A"].fullscreen=true;advanced.fullscreen.insert("A");advanced.Reset(0);advanced.Tick(10000,{});
    Check(advanced.nodes["A"].state==State::Suppressed,"fullscreen inhibition independent of media");
    advanced.config.monitors["A"].blackAfter=5;advanced.Manual("A",10000,true,1);advanced.Input(11000,{},false);advanced.Tick(15000,{});
    Check(advanced.nodes["A"].state==State::Black&&advanced.nodes["A"].sticky,"sticky survives activity and transitions to black");advanced.Reset(16000);Check(!advanced.Any()&&!advanced.nodes["A"].sticky,"stop always cancels sticky");
    advanced.config.perInput=false;advanced.config.monitors["A"].input=1;advanced.Manual("A",20000);advanced.Input(21000,{"B"},true);Check(Running(advanced.nodes["A"].state),"local input override global mode");
    Check(!PowerDue(pref,State::Desktop,0,20000)&&!PowerDue(pref,State::Black,10000,9999)&&PowerDue(pref,State::Black,0,12000),"hardware deadline requires presentation and monotonic time");
    pref.hardware=false;Check(!PowerDue(pref,State::Black,0,999999),"hardware opt-in mandatory");
    PowerDeadline deadline{1000};Check(deadline.Sample(5999,false,false)==0&&deadline.Sample(6000,false,false)==1,"query deadline begins recovery rather than killing wake");
    Check(deadline.Sample(10999,false,false)==0&&deadline.Sample(11000,false,false)==2,"recovery gets a separate full grace");
    deadline={1000};Check(deadline.Sample(2000,false,true)==0&&deadline.Sample(999999,false,true)==0,"accepted off can remain until activity");
    Check(deadline.Sample(1000000,true,true)==1&&deadline.Sample(1005000,true,true)==2,"stop starts independent wake grace");
    Check(Parse("version=1\ntimeout=1\npoll=9999999\npadding=9999\n",copy,error)&&copy.timeout==5&&copy.poll==10000&&copy.padding==1024,"range validation");
    int ignored=0; Config imported;
    imported.automatic=true;imported.monitors["A"].hardware=true;imported.monitors["A"].powerAfter=10;
    Profile importedProfile;importedProfile.policy.automatic=true;importedProfile.policy.monitors["A"].hardware=true;importedProfile.policy.monitors["A"].powerAfter=20;imported.profiles.push_back(importedProfile);
    Check(Import("idleTimeout=3600\ncheckInterval=10000\npixelShiftCompensation=1024\nmonitorEnabled_alias=0 ; disabled\nmonitor1=0\nmonitorEnabled_disconnected=0\naudioDetectionEnabled=0\nstartupEnabled=1\nunknown=3\n",{{"A","alias"},{"B","alias2"}},imported,ignored,error)&&!imported.monitors["A"].enabled&&!imported.monitors["B"].enabled&&!imported.monitors["disconnected"].enabled&&imported.padding==1024&&!imported.media&&!imported.automatic&&ignored==1,"legacy import exact preferences with automation withheld for explicit review");
    Check(!imported.monitors["A"].hardware&&!imported.monitors["A"].powerAfter&&!imported.profiles[0].policy.automatic&&!imported.profiles[0].policy.monitors["A"].hardware&&!imported.profiles[0].policy.monitors["A"].powerAfter,"legacy import revokes base and profile hardware authority");
    Check(!Import("idleTimeout=invalid\n",{},imported,ignored,error),"malformed legacy fails transaction");
    Check(!Import("monitorBogus=1\n",{},imported,ignored,error),"unknown-only monitor key is not a successful import");
    auto r=Padded({-3840,-200,0,1960},8);
    Check(r.left==-3848&&r.top==-208&&r.right==8&&r.bottom==1968,"negative coordinates physical padding; DPI independent");
    Check(!MediaOverlap({0,0,1928,1080},{1920,0,3840,1080}),"invisible 8px border does not suppress adjacent monitor");
    Check(MediaOverlap({-1920,0,400,1080},{0,0,1920,1080}),"substantive spanning media maps both displays");
    Check(Canonical("\\\\?\\DISPLAY#ABC")=="\\\\?\\display#abc","case-insensitive Windows monitor identity");
    for(bool active:{false,true}) for(bool includeMuted:{false,true}) for(bool sessionMute:{false,true}) for(bool deviceMute:{false,true}) for(float peak:{0.0f,0.5f}) for(float volume:{0.0f,1.0f}) for(float endpoint:{0.0f,1.0f})
        Check(AudioBlocks(active,includeMuted,sessionMute,deviceMute,peak,volume,endpoint)==(active&&(includeMuted||(!sessionMute&&!deviceMute&&peak>0&&volume>0&&endpoint>0))),"injected audio active/mute/peak/endpoint policy");
    Check(DisplayRequestBlocks(true,false,true)&&!DisplayRequestBlocks(true,true,true)&&!DisplayRequestBlocks(false,false,true),"silent display request policy excludes owned saver aggregate");
    MediaGrace grace;Check(grace.Apply({100,true,true,false,{"A"}},1000).any,"media grace capture");
    Check(grace.Apply({2100,true,false,false,{}},1000).any,"quiet passage grace boundary");
    Check(!grace.Apply({2101,true,false,false,{}},1000).any,"quiet passage grace expires without self refresh");
    Check(!grace.Apply({1500,false,false,false,{}},1000).valid,"query error is never hidden by grace");
    for(bool perInput:{false,true}) for(bool media:{false,true}) for(bool perMedia:{false,true}) for(bool muted:{false,true}) for(int poll:{250,1000,10000}) {
        Controller p; p.config.automatic=true;p.config.timeout=5;p.config.poll=poll;p.config.perInput=perInput;p.config.media=media;p.config.perMedia=perMedia;p.config.muted=muted;
        p.config.monitors={{"A",{true,1}},{"B",{true,2}}};p.Topology({"A","B"},0);
        Media quiet{4999,true,false,false,{}};p.Tick(4999,quiet);Check(!p.Any(),"before idle boundary");
        quiet.at=5000;p.Tick(5000,quiet);Check(p.nodes["A"].state==State::Launching&&p.nodes["B"].state==State::Launching,"idle boundary");
        auto old=p.nodes["B"].generation;
        p.Input(5100,{"A"},true);
        Check(p.nodes["A"].state==State::Desktop&&(Running(p.nodes["B"].state)==perInput),"mouse input independent/global");
        Check(p.Result("B",old,false)==perInput,"generation rejects stale launch result");
        quiet.at=10000;p.Tick(10000,quiet);Check(p.nodes["A"].state==State::Desktop,"between-poll input survives 10s polling");
        p.Input(10100,{"A","B"},true);Check(!p.Any(),"keyboard cursor plus foreground");
        p.Manual("A",10101);p.Input(10102,{"A"},true,10100);Check(p.Any(),"queued click preceding manual start cannot immediately dismiss it");
        p.Input(10102,{},false);Check(p.Any()==perInput,"unknown input preserves independent presentation, shared mode wakes");p.Input(10103,{"A"},true);Check(!p.Any(),"attributed input dismisses independent presentation");
        p.Reset(0);Media playing{5000,true,true,false,{"A"}};p.Tick(5000,playing);
        Check(Running(p.nodes["A"].state)==!media,"global media setting");
        Check(Running(p.nodes["B"].state)==(!media||perMedia),"per monitor media setting");
        p.Reset(0);playing.ambiguous=true;p.Tick(5000,playing);Check(p.Any()==!media,"ambiguous media conservative");
        p.Reset(0);playing.valid=false;p.Tick(5000,playing);Check(p.Any()==!media,"failed media conservative");
        p.Reset(0);playing.valid=true;playing.at=0;p.Tick(30000,playing);Check(p.Any()==!media,"stale media conservative");
        p.Reset(20000);quiet.at=10000;p.Tick(10000,quiet);Check(!p.Any(),"clock regression no underflow activation");
    }
    Controller p;p.config.automatic=true;p.config.media=false;p.config.timeout=5;p.config.monitors=c.monitors;
    p.Topology({"display-B","display-A"},0);p.Tick(5000,{});Check(!p.Any(),"all disabled never enabled by fallback");
    p.Topology({"disconnected-\xc3\xa9","display-A"},6000);p.Tick(11000,{});Check(p.nodes.begin()->second.state==State::Launching,"reconnect preserves saver preference");
    auto generation=p.nodes.begin()->second.generation;p.Result(p.nodes.begin()->first,generation,false);
    for(int i=0;i<100;++i) p.Tick(12000+i*10000,{});
    Check(p.nodes.begin()->second.state==State::Fallback&&p.nodes.begin()->second.generation==generation,"failure no relaunch loop");
    p.paused=true;p.Tick(999999,{});Check(!p.Any(),"pause stops sessions");
    p.paused=false;p.blocked=true;p.Manual("disconnected-\xc3\xa9",1000000);Check(!p.Any(),"power/session block prevents manual");
    p.blocked=false;p.Reset(1000000);p.Tick(1004999,{});Check(!p.Any(),"resume grace timeout");
    // Eight hours of virtual policy, including topology churn and stale completions.
    p.config.monitors.clear();p.config.perInput=true;p.Topology({"A","B","C"},0);
    for(Time t=0;t<8ULL*60*60*1000;t+=250) {
        if(t%7000==0)p.Input(t,{"A"},true);if(t%11000==0)p.Input(t,{"B"},true);
        if(t%60000==0)p.Topology({"C","A","B"},t);
        p.Tick(t,{});Check(p.nodes.size()==3,"virtual soak bounded controller state");
    }
    printf("POLICY PASS: %d assertions; 48 input/media/mute/poll combinations; 8-hour virtual-time controller soak (not display soak)\n",checks);
}
