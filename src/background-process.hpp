// SPDX-License-Identifier: MIT
// Included inside namespace dac after Handle, Wide, and Utf8. No UI dependencies.
#pragma once

class BackgroundProcessWorker {
    struct Jobs {
        std::vector<HANDLE> handles;bool complete=true;
        ~Jobs(){for(auto job:handles)CloseHandle(job);}
    };
    std::mutex mutex_;std::condition_variable changed_;std::thread worker_;
    bool stopped_=false,pending_=false,ready_=false,enabled_=false;Time requestedEpoch_=0;
    BackgroundRequest pendingRequest_;BackgroundObservation result_;
    std::shared_ptr<Jobs> pendingJobs_;
    const std::function<BackgroundScope(HANDLE)> processScope_;
    static Time MicrosNow(){return static_cast<Time>(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());}
    static bool TokenUserBytes(HANDLE process,std::vector<BYTE>& bytes,DWORD* session=nullptr) {
        HANDLE raw=nullptr;if(!OpenProcessToken(process,TOKEN_QUERY,&raw))return false;Handle token(raw);DWORD required=0;
        GetTokenInformation(token,TokenUser,nullptr,0,&required);
        if(!required||required>65536)return false;bytes.resize(required);
        if(!GetTokenInformation(token,TokenUser,bytes.data(),required,&required))return false;
        if(session){DWORD bytesRead=0;if(!GetTokenInformation(token,TokenSessionId,session,sizeof(*session),&bytesRead))return false;}
        return true;
    }
    static bool ImagePath(HANDLE process,std::string& path) {
        std::vector<wchar_t> buffer(32768);DWORD length=static_cast<DWORD>(buffer.size());
        if(!QueryFullProcessImageNameW(process,0,buffer.data(),&length)||!length||length>=buffer.size())return false;
        path=Utf8(std::wstring(buffer.data(),length));return !path.empty();
    }
    static BackgroundObservation Scan(const BackgroundRequest& request,const std::shared_ptr<Jobs>& jobs,
            const std::function<BackgroundScope(HANDLE)>& processScope) {
        const Time began=MicrosNow(),observedAt=GetTickCount64();std::vector<BackgroundProcessIdentity> catalog;
        // Authority begins when the scan began. A slow scan cannot publish an
        // old process identity with a newly minted positive-grace timestamp.
        auto finish=[&](bool complete){auto out=MatchBackgroundCatalog(request,observedAt,catalog,complete);out.durationMicros=MicrosNow()-began;return out;};
        if(!HasBackgroundRules(request.rules))return finish(false);
        DWORD ownSession=0;std::vector<BYTE> ownUser;std::string ownPath;
        if(!ProcessIdToSessionId(GetCurrentProcessId(),&ownSession)||!TokenUserBytes(GetCurrentProcess(),ownUser)||!ImagePath(GetCurrentProcess(),ownPath))return finish(false);
        const auto ownSid=reinterpret_cast<TOKEN_USER*>(ownUser.data())->User.Sid;
        Handle snapshot(CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0));if(!snapshot)return finish(false);
        PROCESSENTRY32W row{};row.dwSize=sizeof(row);
        if(!Process32FirstW(snapshot,&row))return finish(GetLastError()==ERROR_NO_MORE_FILES);
        bool complete=true;
        do {
            if(catalog.size()>=kBackgroundProcessLimit){complete=false;break;}
            BackgroundProcessIdentity entry;entry.pid=row.th32ProcessID;entry.name=Utf8(row.szExeFile);
            if(!entry.pid||entry.pid==GetCurrentProcessId()){entry.scope=BackgroundScope::Excluded;catalog.push_back(std::move(entry));continue;}
            DWORD session=0;
            if(!ProcessIdToSessionId(entry.pid,&session)){catalog.push_back(std::move(entry));continue;}
            if(session!=ownSession){entry.scope=BackgroundScope::Excluded;catalog.push_back(std::move(entry));continue;}
            Handle process(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION|SYNCHRONIZE,FALSE,entry.pid));
            if(!process){catalog.push_back(std::move(entry));continue;}
            auto alive=WaitForSingleObject(process,0);
            if(alive!=WAIT_TIMEOUT){if(alive==WAIT_OBJECT_0)entry.scope=BackgroundScope::Excluded;catalog.push_back(std::move(entry));continue;}
            std::vector<BYTE> user;
            if(!TokenUserBytes(process,user,&session)){catalog.push_back(std::move(entry));continue;}
            // Check the retained process token as well as the preliminary PID
            // lookup: reuse must not smuggle another session into this catalog.
            if(session!=ownSession){entry.scope=BackgroundScope::Excluded;catalog.push_back(std::move(entry));continue;}
            if(!EqualSid(ownSid,reinterpret_cast<TOKEN_USER*>(user.data())->User.Sid)){entry.scope=BackgroundScope::Excluded;catalog.push_back(std::move(entry));continue;}
            // The live registry callback closes the request/scan race for saver
            // jobs registered after Request. The retained handle also prevents
            // a reused PID from changing the callback's process identity.
            BackgroundScope jobScope=BackgroundScope::Eligible;
            if(processScope)jobScope=processScope(process);
            else {
                bool owned=false,jobKnown=!jobs||jobs->complete;
                if(jobs)for(auto job:jobs->handles){BOOL member=FALSE;if(!IsProcessInJob(process,job,&member))jobKnown=false;else if(member){owned=true;break;}}
                jobScope=owned?BackgroundScope::Excluded:jobKnown?BackgroundScope::Eligible:BackgroundScope::Unknown;
            }
            if(jobScope==BackgroundScope::Excluded){entry.scope=BackgroundScope::Excluded;catalog.push_back(std::move(entry));continue;}
            if(ImagePath(process,entry.app.path)) {
                entry.app.name=BackgroundBasename(entry.app.path);entry.app.known=true;entry.name=entry.app.name;
                if(ExecutableEqual(entry.app.path,ownPath))entry.scope=BackgroundScope::Excluded;
                else if(jobScope==BackgroundScope::Eligible)entry.scope=BackgroundScope::Eligible;
            }
            alive=WaitForSingleObject(process,0);
            if(alive==WAIT_OBJECT_0)entry.scope=BackgroundScope::Excluded;
            else if(alive!=WAIT_TIMEOUT)entry.scope=BackgroundScope::Unknown;
            catalog.push_back(std::move(entry));
        }while(Process32NextW(snapshot,&row));
        if(complete&&GetLastError()!=ERROR_NO_MORE_FILES)complete=false;
        return finish(complete);
    }
    void Main() {
        Time lastScan=0;bool scanned=false;
        std::unique_lock lock(mutex_);
        while(!stopped_) {
            changed_.wait(lock,[&]{return stopped_||pending_;});if(stopped_)break;
            auto now=GetTickCount64();
            if(scanned&&now-lastScan<1000) {
                changed_.wait_for(lock,std::chrono::milliseconds(1000-(now-lastScan)),[&]{return stopped_||!pending_;});
                continue;
            }
            auto request=std::move(pendingRequest_);auto jobs=std::move(pendingJobs_);pending_=false;
            if(!HasBackgroundRules(request.rules))continue;
            lastScan=GetTickCount64();scanned=true;lock.unlock();
            BackgroundObservation result;
            try{result=Scan(request,jobs,processScope_);}catch(...){result.epoch=request.epoch;result.at=lastScan;result.matches.assign(request.rules.size(),BackgroundMatch::Unknown);}
            lock.lock();lastScan=result.at;if(stopped_)break;
            // A policy change invalidates an in-flight result immediately.
            if(enabled_&&requestedEpoch_==request.epoch){result_=std::move(result);ready_=true;}
        }
    }
public:
    // Called only on this worker. The callback must keep any registry lock
    // brief, avoid UI calls, and remain valid until Stop has joined the worker.
    explicit BackgroundProcessWorker(std::function<BackgroundScope(HANDLE)> processScope={}):processScope_(std::move(processScope)){}
    ~BackgroundProcessWorker(){Stop();}
    BackgroundProcessWorker(const BackgroundProcessWorker&)=delete;
    BackgroundProcessWorker& operator=(const BackgroundProcessWorker&)=delete;
    // Without a live callback, call with registry protection while supplying
    // borrowed owned-job handles. Copies outlive the registry's originals.
    // A supplied live callback replaces this immutable job snapshot.
    bool Request(BackgroundRequest request,const std::vector<HANDLE>& excludedJobs={}) {
        if(request.rules.size()>kBackgroundRuleLimit)return false;
        auto jobs=std::make_shared<Jobs>();
        if(!processScope_&&HasBackgroundRules(request.rules)) {
            if(excludedJobs.size()>64)jobs->complete=false;
            for(size_t i=0;i<std::min(excludedJobs.size(),size_t(64));++i){HANDLE copy=nullptr;
                if(DuplicateHandle(GetCurrentProcess(),excludedJobs[i],GetCurrentProcess(),&copy,JOB_OBJECT_QUERY,FALSE,0))jobs->handles.push_back(copy);else jobs->complete=false;}
        }
        std::lock_guard lock(mutex_);if(stopped_)return false;
        requestedEpoch_=request.epoch;enabled_=HasBackgroundRules(request.rules);
        if(ready_&&(result_.epoch!=request.epoch||!enabled_))ready_=false;
        if(!enabled_){pending_=false;pendingJobs_.reset();changed_.notify_one();return true;}
        pendingRequest_=std::move(request);pendingJobs_=std::move(jobs);pending_=true;
        if(!worker_.joinable())try{worker_=std::thread([this]{Main();});}catch(...){pending_=false;pendingJobs_.reset();return false;}
        changed_.notify_one();return true;
    }
    bool Take(BackgroundObservation& output) {
        std::lock_guard lock(mutex_);if(stopped_||!ready_)return false;output=std::move(result_);ready_=false;return true;
    }
    void Stop() {
        {std::lock_guard lock(mutex_);stopped_=true;pending_=ready_=false;pendingJobs_.reset();result_={};changed_.notify_one();}
        if(worker_.joinable())worker_.join();
    }
};
