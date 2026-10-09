// SPDX-License-Identifier: MIT
// Invoked only by dac-test-runner on its private desktop, with a build-local profile.
#include <windows.h>
#include <psapi.h>
#include <cstdio>
#include <cstdint>
#include <string>
#include <algorithm>
struct Child {
    HANDLE process{};
    ~Child(){if(process){if(WaitForSingleObject(process,0)==WAIT_TIMEOUT){TerminateProcess(process,99);WaitForSingleObject(process,5000);}CloseHandle(process);}}
    Child()=default;Child(const Child&)=delete;Child& operator=(const Child&)=delete;
};
bool Launch(const std::wstring& exe,const std::wstring& arguments,Child& child){
    std::wstring command=L"\""+exe+L"\" "+arguments;
    STARTUPINFOW startup{};startup.cb=sizeof(startup);startup.dwFlags=STARTF_USESHOWWINDOW;startup.wShowWindow=SW_HIDE;
    PROCESS_INFORMATION process{};
    if(!CreateProcessW(exe.c_str(),command.data(),nullptr,nullptr,FALSE,0,nullptr,nullptr,&startup,&process))return false;
    child.process=process.hProcess;CloseHandle(process.hThread);return true;
}
bool Successful(Child& child,DWORD limit){
    if(WaitForSingleObject(child.process,limit)!=WAIT_OBJECT_0)return false;
    DWORD code=1;return GetExitCodeProcess(child.process,&code)&&code==0;
}
int wmain(int argc,wchar_t** argv){
    if(argc!=3)return 2;
    wchar_t desktop[256]{};DWORD needed=0;
    if(!GetUserObjectInformationW(GetThreadDesktop(GetCurrentThreadId()),UOI_NAME,desktop,sizeof(desktop),&needed)||!std::wstring(desktop).starts_with(L"DAC-Isolated-Test-")){
        fputs("Refusing host lifecycle outside private test desktop\n",stderr);return 77;
    }
    std::wstring exe=argv[1],base=argv[2];std::replace(exe.begin(),exe.end(),L'/',L'\\');std::replace(base.begin(),base.end(),L'/',L'\\');
    auto profile=base+L"\\lifecycle-profile-"+std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(GetTickCount64());
    if(!CreateDirectoryW(profile.c_str(),nullptr))return 1;
    auto command=L"--background --run-for-seconds 30 --config-directory \""+profile+L"\"";
    Child primary,secondary,stop;auto start=GetTickCount64();
    if(!Launch(exe,command,primary)){fprintf(stderr,"host launch failed %lu\n",GetLastError());return 1;}
    HWND host=nullptr;auto until=GetTickCount64()+10000;
    while(GetTickCount64()<until&&WaitForSingleObject(primary.process,0)==WAIT_TIMEOUT){
        host=FindWindowW(L"DAC-Host",nullptr);DWORD_PTR value=0;
        if(host&&SendMessageTimeoutW(host,WM_NULL,0,0,SMTO_ABORTIFHUNG,200,&value))break;
        Sleep(20);
    }
    if(!host){fputs("host did not create its native controller window\n",stderr);return 1;}
    DWORD owner=0;GetWindowThreadProcessId(host,&owner);
    if(owner!=GetProcessId(primary.process)){fputs("unexpected controller ownership\n",stderr);return 1;}
    auto startupMs=GetTickCount64()-start;Sleep(500);
    FILETIME created{},exited{},kernel0{},user0{},kernel1{},user1{};
    DWORD handles0=0,handles1=0;PROCESS_MEMORY_COUNTERS_EX memory{};
    auto ticks=[](FILETIME f){return (uint64_t(f.dwHighDateTime)<<32)|f.dwLowDateTime;};
    if(!GetProcessTimes(primary.process,&created,&exited,&kernel0,&user0)||!GetProcessHandleCount(primary.process,&handles0))return 1;
    auto gdi0=GetGuiResources(primary.process,GR_GDIOBJECTS);
    auto measured=GetTickCount64();Sleep(5000);
    auto elapsed=GetTickCount64()-measured;
    if(!GetProcessTimes(primary.process,&created,&exited,&kernel1,&user1)||!GetProcessHandleCount(primary.process,&handles1)||
       !GetProcessMemoryInfo(primary.process,reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&memory),sizeof(memory)))return 1;
    auto gdi1=GetGuiResources(primary.process,GR_GDIOBJECTS);
    double cpuMs=double(ticks(kernel1)+ticks(user1)-ticks(kernel0)-ticks(user0))/10000.0;
    printf("STANDALONE idle interval_ms=%llu CPU_ms=%.3f one_core_percent=%.4f working_set_bytes=%zu private_bytes=%zu handles=%lu->%lu GDI=%lu->%lu startup_ms=%llu (shipping DAC.exe, private desktop without Explorer tray, automation off)\n",elapsed,cpuMs,100.0*cpuMs/elapsed,static_cast<size_t>(memory.WorkingSetSize),static_cast<size_t>(memory.PrivateUsage),handles0,handles1,gdi0,gdi1,startupMs);
    if(!Launch(exe,command,secondary)||!Successful(secondary,3000)||WaitForSingleObject(primary.process,0)!=WAIT_TIMEOUT){
        fputs("single-instance launch did not preserve original owner\n",stderr);return 1;
    }
    auto began=GetTickCount64();
    if(!Launch(exe,L"--stop",stop)||!Successful(stop,15000)||!Successful(primary,3000)){
        fputs("scoped --stop failed to close original owner cleanly\n",stderr);return 1;
    }
    if(IsWindow(host)){fputs("native owner window survived shutdown\n",stderr);return 1;}
    printf("STANDALONE lifecycle PASS: shipping executable, private desktop, unique local profile, native ownership, second-instance no-op, scoped stop; stop_ms=%llu\n",GetTickCount64()-began);
    return 0;
}
