// SPDX-License-Identifier: MIT
// Copyright (c) 2026 StarlightDaemon
#include "dac.cpp"
#include "installation-lease.hpp"

namespace {
void Print(const char* text,bool error=false) {
    HANDLE stream=GetStdHandle(error?STD_ERROR_HANDLE:STD_OUTPUT_HANDLE);
    if(!stream||stream==INVALID_HANDLE_VALUE){AttachConsole(ATTACH_PARENT_PROCESS);stream=GetStdHandle(error?STD_ERROR_HANDLE:STD_OUTPUT_HANDLE);}
    DWORD written=0;if(stream&&stream!=INVALID_HANDLE_VALUE)WriteFile(stream,text,static_cast<DWORD>(strlen(text)),&written,nullptr);
}
bool UnsignedNumber(const wchar_t* value,DWORD& number) {
    if(!value||!*value)return false;unsigned long long result=0;
    for(auto c= value;*c;++c){if(*c<L'0'||*c>L'9')return false;result=result*10+static_cast<unsigned>(*c-L'0');if(result>MAXDWORD)return false;}
    number=static_cast<DWORD>(result);return true;
}
int RequestStop() {
    if(dac::objectScope.empty())return 3;
    dac::Handle event(OpenEventW(EVENT_MODIFY_STATE,FALSE,dac::kStopName));
    if(!event)return GetLastError()==ERROR_FILE_NOT_FOUND?0:3;
    if(!SetEvent(event))return 3;
    const auto until=GetTickCount64()+12000;
    do {dac::Handle singleton(OpenMutexW(SYNCHRONIZE,FALSE,(dac::objectScope+L"-Singleton").c_str()));
        if(!singleton&&GetLastError()==ERROR_FILE_NOT_FOUND)return 0;Sleep(25);
    }while(GetTickCount64()<until);
    Print("DAC stop requested; controller exit could not be confirmed.\n",true);return 4;
}
struct Supervisor {
    HANDLE finished=CreateEventW(nullptr,TRUE,FALSE,nullptr);
    HANDLE thread=nullptr;
    ULONGLONG started=GetTickCount64();
    static DWORD WINAPI Run(void* context) {
        auto& self=*static_cast<Supervisor*>(context);
        while(WaitForSingleObject(self.finished,25)==WAIT_TIMEOUT) {
            const auto now=GetTickCount64(),stopping=dac::shutdownRequestedAt.load();
            if((stopping&&now-stopping>=8000)||(!dac::startupComplete&&now-self.started>=30000)) {
                // OS teardown closes owned jobs and windows even when a worker is
                // stuck in COM, decoding, filesystem or a driver. Fault tickets
                // remain on disk; uncertain hardware is never silently cleared.
                Print("DAC exceeded its shutdown/startup deadline; terminating this owned process.\n",true);
                TerminateProcess(GetCurrentProcess(),124);
            }
        }
        return 0;
    }
    bool Start(){if(!finished)return false;thread=CreateThread(nullptr,0,Run,this,0,nullptr);return thread!=nullptr;}
    void Finish(){if(finished)SetEvent(finished);if(thread){WaitForSingleObject(thread,1000);CloseHandle(thread);thread=nullptr;}}
    ~Supervisor(){Finish();if(finished)CloseHandle(finished);}
};
int HardwareHelper(int count,wchar_t** args) {
    // A helper is private protocol, never an arbitrary device/command launcher.
    if((count!=5&&count!=7)||dac::objectScope.empty())return 10;
    if(count==7&&(wcscmp(args[5],L"--config-directory")!=0||!dac::SetConfigDirectory(args[6])))return 10;
    std::string id,encoded=dac::Utf8(args[2]),nonce=dac::Utf8(args[3]);DWORD parentId=0;
    if(encoded.size()>4096||!dac::Unhex(encoded,id)||id.empty()||!dac::ValidUtf8(id)||id.find_first_of("\r\n")!=std::string::npos||
       nonce.size()!=32||nonce.find_first_not_of("0123456789abcdef")!=std::string::npos||!UnsignedNumber(args[4],parentId)||!parentId||parentId==GetCurrentProcessId())return 10;
    DWORD currentSession=0,parentSession=0;
    if(!ProcessIdToSessionId(GetCurrentProcessId(),&currentSession)||!ProcessIdToSessionId(parentId,&parentSession)||currentSession!=parentSession)return 10;
    dac::Handle parent(OpenProcess(SYNCHRONIZE|PROCESS_QUERY_LIMITED_INFORMATION,FALSE,parentId));if(!parent)return 10;
    std::vector<wchar_t> executable(32768),owner(32768);DWORD ownerSize=32768;
    const auto size=GetModuleFileNameW(nullptr,executable.data(),32768);
    if(!size||size>=32768||!QueryFullProcessImageNameW(parent,0,owner.data(),&ownerSize)||CompareStringOrdinal(executable.data(),static_cast<int>(size),owner.data(),static_cast<int>(ownerSize),TRUE)!=CSTR_EQUAL||WaitForSingleObject(parent,0)!=WAIT_TIMEOUT)return 10;
    dac::Handle cancel(OpenEventW(SYNCHRONIZE,FALSE,dac::PowerCancelName(nonce).c_str()));if(!cancel)return 10;
    // PowerGuardian checks the nonce-bound ticket and saved per-monitor opt-in
    // again before acquiring a physical monitor or issuing a power command.
    return dac::PowerGuardian(id,nonce,parent);
}
int Main(int count,wchar_t** args) {
    if(!SetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_SYSTEM32))return 3;
    if(count==2&&wcscmp(args[1],L"--help")==0){Print(
        "DAC 0.1.0 - Display Activity Controls\n"
        "Usage: DAC.exe [--background] [--run-for-seconds 1..86400]\n"
        "               [--config-directory <existing absolute local directory>]\n"
        "       DAC.exe --help | --version | --stop\n"
        "A normal start opens Quick setup on first run; automation is initially off.\n"
        "Right-click the tray for protection, settings, appearance and optional login startup.\n"
        "--config-directory selects a separate profile; shares, mapped drives and reparse paths are rejected.\n"
        "Emergency exit: Ctrl+Alt+Shift+F12. --stop affects this user/session only.\n");return 0;}
    if(count==2&&wcscmp(args[1],L"--version")==0){Print("DAC 0.1.0\n");return 0;}
    if(count==2&&wcscmp(args[1],L"--stop")==0)return RequestStop();
    if(count>1&&wcscmp(args[1],L"--power-helper")==0)return HardwareHelper(count,args);
    DWORD lifetime=0;
    for(int i=1;i<count;++i) {
        if(wcscmp(args[i],L"--background")==0)dac::backgroundStart=true;
        else if(wcscmp(args[i],L"--config-directory")==0&&i+1<count){if(!dac::SetConfigDirectory(args[++i])){Print("Configuration directory must already exist on a fixed local drive without redirection.\n",true);return 2;}}
        else if(wcscmp(args[i],L"--run-for-seconds")==0&&i+1<count){if(!UnsignedNumber(args[++i],lifetime)||!lifetime||lifetime>86400)return 2;}
        else {Print("Unknown or malformed argument. Use DAC.exe --help.\n",true);return 2;}
    }
    DWORD session=0;if(!ProcessIdToSessionId(GetCurrentProcessId(),&session)||session==0||dac::objectScope.empty())return 3;
    // Treat a second launch as a successful no-op; never start duplicate owners.
    {dac::Handle existing(OpenMutexW(SYNCHRONIZE,FALSE,(dac::objectScope+L"-Singleton").c_str()));if(existing)return 0;}
    Supervisor supervisor;if(!supervisor.Start())return 3;
    if(!dac::Initialize()){
        dac::Shutdown();supervisor.Finish();Print("DAC could not initialize. Check tray availability, local profile permissions and the emergency hotkey.\n",true);
        if(!dac::backgroundStart&&!GetConsoleWindow()&&GetFileType(GetStdHandle(STD_ERROR_HANDLE))!=FILE_TYPE_PIPE)
            MessageBoxW(nullptr,L"DAC could not start.\n\nExit any predecessor DAC controller, check that Ctrl+Alt+Shift+F12 is available, and confirm your local profile folder is writable.\n\nDAC has stopped its initialized workers and closed its presentation windows. Retry after resolving the conflict.",L"DAC startup failed",MB_OK|MB_ICONERROR);
        return 1;
    }
    auto started=GetTickCount64();
    while(WaitForSingleObject(dac::stopEvent,50)==WAIT_TIMEOUT) {
        if(lifetime&&GetTickCount64()-started>=static_cast<ULONGLONG>(lifetime)*1000){SetEvent(dac::stopEvent);break;}
        if(WaitForSingleObject(dac::uiThread,0)!=WAIT_TIMEOUT){SetEvent(dac::stopEvent);break;}
    }
    dac::Shutdown();return 0;
}
} // namespace

int WINAPI wWinMain(_In_ HINSTANCE,_In_opt_ HINSTANCE,_In_ PWSTR,_In_ int) {
    std::vector<wchar_t> executable(32768);DWORD length=GetModuleFileNameW(nullptr,executable.data(),32768);
    if(!length||length>=32768)return 5;
    std::wstring directory(executable.data(),length);auto separator=directory.find_last_of(L"\\");
    if(separator==std::wstring::npos)return 5;directory.resize(separator);
    // Retain until OS teardown, beyond Shutdown and singleton release. Each
    // helper acquires its own lease before any argument or hardware operation.
    static HANDLE processLifetimeLease=nullptr;
    if(!dac::AcquireInstallationLease(directory,processLifetimeLease)||
       (processLifetimeLease&&!dac::LocalPath(directory,true,true)))return 5;
    int count=0;wchar_t** args=CommandLineToArgvW(GetCommandLineW(),&count);if(!args)return 3;
    int result=Main(count,args);LocalFree(args);return result;
}
