// SPDX-License-Identifier: MIT
// Included inside namespace dac after AppRule, AppIdentity, and RuleMatch.
#pragma once

constexpr Time kBackgroundGraceMs=5000;
constexpr size_t kBackgroundRuleLimit=32,kBackgroundProcessLimit=8192;
enum class BackgroundMatch { Unknown,Absent,Present };
struct BackgroundRequest {Time epoch=0;std::vector<AppRule> rules;};
struct BackgroundObservation {
    Time epoch=0,at=0;std::vector<BackgroundMatch> matches;
    size_t inspected=0;Time durationMicros=0;
};
bool HasBackgroundRules(const std::vector<AppRule>& rules) {
    return rules.size()<=kBackgroundRuleLimit&&std::any_of(rules.begin(),rules.end(),[](const AppRule& r){return r.enabled&&r.kind==2;});
}
// Unknown scope covers access failures. Excluded covers other users/sessions,
// DAC itself, its executable siblings, and processes in DAC-owned saver jobs.
enum class BackgroundScope { Unknown,Excluded,Eligible };
struct BackgroundProcessIdentity {
    uint32_t pid=0;AppIdentity app;BackgroundScope scope=BackgroundScope::Unknown;
    std::string name; // Snapshot basename; never a window title or command line.
};
std::string BackgroundBasename(const std::string& path) {
    auto slash=path.find_last_of("/\\");return slash==std::string::npos?path:path.substr(slash+1);
}
BackgroundObservation MatchBackgroundCatalog(const BackgroundRequest& request,Time at,
        const std::vector<BackgroundProcessIdentity>& catalog,bool complete=true) {
    BackgroundObservation out;out.epoch=request.epoch;out.at=at;
    out.matches.assign(std::min(request.rules.size(),kBackgroundRuleLimit),BackgroundMatch::Unknown);
    if(request.rules.size()>kBackgroundRuleLimit)return out;
    out.inspected=std::min(catalog.size(),kBackgroundProcessLimit);
    complete=complete&&catalog.size()<=kBackgroundProcessLimit;
    for(size_t i=0;i<request.rules.size();++i) {
        const auto& rule=request.rules[i];if(!rule.enabled||rule.kind!=2)continue;
        bool unknown=!complete,present=false;
        for(size_t j=0;j<out.inspected;++j) {
            const auto& process=catalog[j];if(process.scope==BackgroundScope::Excluded)continue;
            // Only an identity read from a retained handle can rule out a match.
            // A Toolhelp basename may belong to a PID that has already been reused.
            if(process.app.known&&!ExecutableEqual(process.app.name,BackgroundBasename(rule.executable)))continue;
            if(process.scope!=BackgroundScope::Eligible||!process.app.known){unknown=true;continue;}
            if(RuleMatch(rule,process.app)){present=true;break;}
        }
        out.matches[i]=present?BackgroundMatch::Present:unknown?BackgroundMatch::Unknown:BackgroundMatch::Absent;
    }
    return out;
}
class BackgroundTracker {
    struct Entry {BackgroundMatch match=BackgroundMatch::Unknown;Time at=0,positiveAt=0;bool observed=false,positive=false;};
    Time epoch_=0;std::vector<Entry> entries_;
public:
    void Reset(Time epoch,size_t ruleCount) {epoch_=epoch;entries_.assign(std::min(ruleCount,kBackgroundRuleLimit),{});}
    void Reconfigure(Time epoch,const std::vector<AppRule>& before,const std::vector<AppRule>& after) {
        auto saved=entries_;Reset(epoch,after.size());
        for(size_t i=0;i<entries_.size();++i)for(size_t j=0;j<before.size()&&j<saved.size();++j){auto& a=after[i];auto& b=before[j];
            if(a.kind==b.kind&&a.global==b.global&&a.nameOnly==b.nameOnly&&a.executable==b.executable&&a.enabled==b.enabled&&a.targets==b.targets){entries_[i]=saved[j];break;}}
    }
    Time Epoch()const{return epoch_;}
    bool Apply(const BackgroundObservation& observation,Time now) {
        if(observation.epoch!=epoch_||observation.at>now||now-observation.at>kBackgroundGraceMs||observation.matches.size()!=entries_.size())return false;
        for(const auto& entry:entries_)if(entry.observed&&observation.at<entry.at)return false;
        for(size_t i=0;i<entries_.size();++i) {
            auto& entry=entries_[i];entry.match=observation.matches[i];entry.at=observation.at;entry.observed=true;
            if(entry.match==BackgroundMatch::Present){entry.positive=true;entry.positiveAt=observation.at;}
            else if(entry.match==BackgroundMatch::Absent)entry.positive=false;
        }
        return true;
    }
    bool Active(size_t index,Time now)const {
        if(index>=entries_.size())return false;const auto& entry=entries_[index];
        return entry.positive&&now>=entry.positiveAt&&now-entry.positiveAt<=kBackgroundGraceMs;
    }
    bool Uncertain(size_t index,Time now)const {
        if(index>=entries_.size())return true;const auto& entry=entries_[index];
        return !entry.observed||entry.match==BackgroundMatch::Unknown||now<entry.at||now-entry.at>kBackgroundGraceMs;
    }
};
