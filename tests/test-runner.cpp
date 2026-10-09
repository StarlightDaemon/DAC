// SPDX-License-Identifier: MIT
// Runs reviewed fixtures on a private desktop. Never switches the input desktop.
// A desktop and job provide UI/lifetime isolation, not a security sandbox.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <cstdio>
#include <algorithm>
#include <string>
#include <vector>

struct Handle {
    HANDLE h{};
    ~Handle() { if (h && h != INVALID_HANDLE_VALUE) CloseHandle(h); }
};
std::wstring Quote(const std::wstring& value) {
    std::wstring out=L"\""; size_t slashes=0;
    for (auto c:value) {
        if (c==L'\\') { ++slashes; continue; }
        if (c==L'\"') out.append(slashes*2+1,L'\\');
        else out.append(slashes,L'\\');
        slashes=0; out+=c;
    }
    out.append(slashes*2,L'\\'); return out+L'\"';
}
int wmain(int argc,wchar_t** argv) {
    int expectedExit=0;
    if(argc>=4&&std::wstring(argv[1])==L"--expect-exit"){
        wchar_t* end=nullptr;auto code=wcstol(argv[2],&end,10);
        if(!end||*end||code<0||code>255)return 2;
        expectedExit=static_cast<int>(code);argc-=2;argv+=2;
    }
    if (argc<2) { fputs("test-runner requires an absolute fixture executable\n",stderr); return 2; }
    std::wstring executable=argv[1];std::replace(executable.begin(),executable.end(),L'/',L'\\');
    if (executable.size()<3 || executable[1]!=L':' || (executable[2]!=L'\\' && executable[2]!=L'/')) return 2;
    std::wstring desktopName=L"DAC-Isolated-Test-"+std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(GetTickCount64());
    HDESK desktop=CreateDesktopW(desktopName.c_str(),nullptr,nullptr,0,GENERIC_ALL,nullptr);
    if (!desktop) { fprintf(stderr,"Private desktop unavailable: %lu; test NOT RUN\n",GetLastError()); return 77; }
    int result=1;
    std::wstring hostDeathReceipt;
    {
        Handle job{CreateJobObjectW(nullptr,nullptr)};
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
        limits.BasicLimitInformation.LimitFlags=JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        if (!job.h || !SetInformationJobObject(job.h,JobObjectExtendedLimitInformation,&limits,sizeof(limits))) {
            fprintf(stderr,"Test job unavailable: %lu\n",GetLastError()); CloseDesktop(desktop); return 77;
        }
        SECURITY_ATTRIBUTES sa{sizeof(sa),nullptr,TRUE};
        Handle nullInput{CreateFileW(L"NUL",GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,&sa,OPEN_EXISTING,0,nullptr)};
        Handle output,error;
        if (!DuplicateHandle(GetCurrentProcess(),GetStdHandle(STD_OUTPUT_HANDLE),GetCurrentProcess(),&output.h,0,TRUE,DUPLICATE_SAME_ACCESS) ||
            !DuplicateHandle(GetCurrentProcess(),GetStdHandle(STD_ERROR_HANDLE),GetCurrentProcess(),&error.h,0,TRUE,DUPLICATE_SAME_ACCESS)) {
            fprintf(stderr,"Test output unavailable: %lu\n",GetLastError()); CloseDesktop(desktop); return 77;
        }
        SIZE_T bytes=0; InitializeProcThreadAttributeList(nullptr,1,0,&bytes);
        std::vector<unsigned char> storage(bytes);
        auto attributes=reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(storage.data());
        HANDLE inherited[]={nullInput.h,output.h,error.h};
        if (!InitializeProcThreadAttributeList(attributes,1,0,&bytes)) { CloseDesktop(desktop); return 77; }
        if (!UpdateProcThreadAttribute(attributes,0,PROC_THREAD_ATTRIBUTE_HANDLE_LIST,inherited,sizeof(inherited),nullptr,nullptr)) {
            DeleteProcThreadAttributeList(attributes); CloseDesktop(desktop); return 77;
        }
        STARTUPINFOEXW startup{}; startup.StartupInfo.cb=sizeof(startup);
        startup.StartupInfo.lpDesktop=desktopName.data();
        startup.StartupInfo.dwFlags=STARTF_USESTDHANDLES|STARTF_USESHOWWINDOW;
        startup.StartupInfo.wShowWindow=SW_HIDE;
        startup.StartupInfo.hStdInput=nullInput.h;startup.StartupInfo.hStdOutput=output.h;startup.StartupInfo.hStdError=error.h;
        startup.lpAttributeList=attributes;
        std::wstring command=Quote(executable);
        for (int i=2;i<argc;++i) command+=L" "+Quote(argv[i]);
        if(argc==3&&std::wstring(argv[2])==L"--host-death"){
            hostDeathReceipt=executable+L".host-death-"+std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(GetTickCount64())+L".txt";
            Handle reserved{CreateFileW(hostDeathReceipt.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr)};
            if(!reserved.h||reserved.h==INVALID_HANDLE_VALUE){DeleteProcThreadAttributeList(attributes);CloseDesktop(desktop);return 77;}
            command+=L" "+Quote(hostDeathReceipt);
        }
        PROCESS_INFORMATION process{};
        BOOL created=CreateProcessW(executable.c_str(),command.data(),nullptr,nullptr,TRUE,
            CREATE_SUSPENDED|CREATE_NO_WINDOW|EXTENDED_STARTUPINFO_PRESENT,nullptr,nullptr,&startup.StartupInfo,&process);
        DeleteProcThreadAttributeList(attributes);
        if (!created) fprintf(stderr,"Fixture launch failed: %lu\n",GetLastError());
        else {
            Handle child{process.hProcess},thread{process.hThread};
            if (!AssignProcessToJobObject(job.h,child.h)) {
                fprintf(stderr,"Fixture containment failed: %lu\n",GetLastError()); TerminateProcess(child.h,1); WaitForSingleObject(child.h,5000);
            } else if (ResumeThread(thread.h)==DWORD(-1)) { TerminateJobObject(job.h,1); WaitForSingleObject(child.h,5000); }
            else {
                if (argc==3 && std::wstring(argv[2])==L"--host-death") {
                    std::vector<HANDLE> descendants;
                    auto until=GetTickCount64()+8000;
                    while (descendants.size()!=2 && GetTickCount64()<until) {
                        Handle receipt{CreateFileW(hostDeathReceipt.c_str(),GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,0,nullptr)};
                        char contents[128]{};DWORD read=0,first=0,second=0;
                        if(receipt.h&&receipt.h!=INVALID_HANDLE_VALUE&&ReadFile(receipt.h,contents,127,&read,nullptr)&&sscanf_s(contents,"%lu %lu",&first,&second)==2&&first&&second&&first!=second&&first!=process.dwProcessId&&second!=process.dwProcessId){
                            for(auto id:{first,second}){
                                auto handle=OpenProcess(SYNCHRONIZE|PROCESS_QUERY_LIMITED_INFORMATION,FALSE,id);
                                if(handle)descendants.push_back(handle);
                            }
                            break;
                        }
                        Sleep(20);
                    }
                    if (descendants.size()==2&&WaitForSingleObject(descendants[0],0)==WAIT_TIMEOUT&&WaitForSingleObject(descendants[1],0)==WAIT_TIMEOUT) {
                        // Outer job remains open: disappearance must come from the
                        // fixture host's own kill-on-close job, not this runner.
                        TerminateProcess(child.h,99); WaitForSingleObject(child.h,5000);
                        result=0;
                        for (auto descendant:descendants) if (WaitForSingleObject(descendant,3000)!=WAIT_OBJECT_0) result=1;
                        puts(result?"FAIL child survived host death":"PASS host death: retained child and grandchild handles signaled");
                    } else { fputs("FAIL host-death fixture did not create exactly two descendants\n",stderr); result=1; }
                    for (auto descendant:descendants) CloseHandle(descendant);
                    TerminateJobObject(job.h,0);
                } else {
                auto status=WaitForSingleObject(child.h,120000);
                if (status!=WAIT_OBJECT_0) {
                    fputs("Fixture exceeded 120 second bound; terminating owned job\n",stderr);
                    TerminateJobObject(job.h,124); WaitForSingleObject(child.h,5000); result=124;
                } else {
                    DWORD code=1; GetExitCodeProcess(child.h,&code); result=static_cast<int>(code);
                }
                // Catch leaked fixture descendants even when the parent exits normally.
                TerminateJobObject(job.h,0);
                }
            }
        }
    }
    if(!hostDeathReceipt.empty())DeleteFileW(hostDeathReceipt.c_str());
    if (!CloseDesktop(desktop) && !result) { fprintf(stderr,"Private desktop cleanup failed: %lu\n",GetLastError()); result=1; }
    if(result!=expectedExit){fprintf(stderr,"Expected fixture exit %d, observed %d\n",expectedExit,result);return 1;}
    return 0;
}
