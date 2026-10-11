// SPDX-License-Identifier: MIT
// Fixture holds the production lease after releasing a synthetic singleton.
#include "../src/installation-lease.hpp"
#include <cstdio>
int wmain(int argc,wchar_t** argv) {
    if(argc!=4)return 2;
    HANDLE lease=nullptr;
    if(!dac::AcquireInstallationLease(argv[1],lease))return 5;
    std::wstring singleton=L"Local\\DAC-Installer-Fixture-"+std::to_wstring(GetCurrentProcessId());
    HANDLE mutex=CreateMutexW(nullptr,FALSE,singleton.c_str());
    if(!mutex)return 3;
    CloseHandle(mutex); // NOT evidence of process exit: lease deliberately survives.
    HANDLE ready=CreateFileW(argv[2],GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_NEW,0,nullptr);
    if(ready==INVALID_HANDLE_VALUE)return 3;
    const char receipt[]="singleton-closed; process-alive; lease-retained\n";
    DWORD written=0;BOOL ok=WriteFile(ready,receipt,sizeof(receipt)-1,&written,nullptr);
    CloseHandle(ready);
    if(!ok||written!=sizeof(receipt)-1)return 3;
    const auto until=GetTickCount64()+60000;
    while(GetFileAttributesW(argv[3])==INVALID_FILE_ATTRIBUTES) {
        if(GetTickCount64()>=until)return 124;
        Sleep(10);
    }
    // OS process teardown owns lease release, matching the shipping entry point.
    return 0;
}
