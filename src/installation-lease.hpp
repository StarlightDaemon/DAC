// SPDX-License-Identifier: MIT
// Setup creates this sidecar. Portable ZIPs have no sidecar. Installed hosts,
// helpers and CLI invocations share read leases; maintenance requires exclusion.
#pragma once
#include <windows.h>
#include <string>
namespace dac {
inline constexpr wchar_t kInstallationLease[]=L".dac-lifecycle.lock";
inline bool AcquireInstallationLease(const std::wstring& directory,HANDLE& lifetime) {
    const auto path=directory+L"\\"+kInstallationLease;
    DWORD attributes=GetFileAttributesW(path.c_str());
    if(attributes==INVALID_FILE_ATTRIBUTES) {
        const DWORD error=GetLastError();
        return error==ERROR_FILE_NOT_FOUND||error==ERROR_PATH_NOT_FOUND;
    }
    if(attributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT))return false;
    HANDLE file=CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT,nullptr);
    if(file==INVALID_HANDLE_VALUE)return false;
    FILE_ATTRIBUTE_TAG_INFO info{};
    if(GetFileType(file)!=FILE_TYPE_DISK||!GetFileInformationByHandleEx(file,FileAttributeTagInfo,&info,sizeof(info))||
       (info.FileAttributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT))) {CloseHandle(file);return false;}
    lifetime=file;return true;
}
}