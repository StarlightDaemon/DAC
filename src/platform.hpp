// SPDX-License-Identifier: MIT
// Copyright (c) 2026 StarlightDaemon
// Selectively ported from verified DAC R01C; see docs/SOURCE_INVENTORY.md.
#pragma once

#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <commdlg.h>
#include <wtsapi32.h>
#include <mmdeviceapi.h>
#include <audiopolicy.h>
#include <endpointvolume.h>
#include <powrprof.h>
#include <dwmapi.h>
#include <physicalmonitorenumerationapi.h>
#include <lowlevelmonitorconfigurationapi.h>
#include <gdiplus.h>
#include <xinput.h>
#include <tlhelp32.h>
#include <sddl.h>

namespace dac {
// Installed MinGW exposes only the meter forward declaration. This minimal
// ABI view uses the documented first method after IUnknown; no SDK code vendored.
struct MeterPeak : IUnknown { virtual HRESULT STDMETHODCALLTYPE GetPeakValue(float*)=0; };
constexpr GUID meterIID={0xc02216f6,0x8c67,0x4b5b,{0x9d,0x00,0xd0,0x08,0xe7,0x3e,0x00,0x64}};
// Independent standalone identities. Local objects are additionally user/session scoped.
constexpr wchar_t kClass[]=L"DAC-Host",kEditor[]=L"DAC-Settings",kOptions[]=L"DAC-SaverOptions";
std::wstring UserSessionScope() {
    HANDLE token=nullptr;DWORD required=0,session=0;
    if(!ProcessIdToSessionId(GetCurrentProcessId(),&session)||!OpenProcessToken(GetCurrentProcess(),TOKEN_QUERY,&token))return {};
    GetTokenInformation(token,TokenUser,nullptr,0,&required);std::vector<BYTE> bytes(required);
    if(!required||!GetTokenInformation(token,TokenUser,bytes.data(),required,&required)){CloseHandle(token);return {};}
    CloseHandle(token);LPWSTR sid=nullptr;
    if(!ConvertSidToStringSidW(reinterpret_cast<TOKEN_USER*>(bytes.data())->User.Sid,&sid))return {};
    std::wstring result=L"Local\\DAC-"+std::wstring(sid)+L"-"+std::to_wstring(session);LocalFree(sid);return result;
}
const std::wstring objectScope=UserSessionScope();
#ifdef DAC_HARNESS
const std::wstring testStopName=L"Local\\DAC-Test-"+std::to_wstring(GetCurrentProcessId());
const wchar_t* kStopName=testStopName.c_str();
#else
const std::wstring stopName=objectScope+L"-Emergency";
const wchar_t* kStopName=stopName.c_str();
#endif
constexpr UINT kTray=WM_APP+1,kReload=WM_APP+2,kQuit=WM_APP+3,kIntegration=WM_APP+4;
// Owned by the UI thread; the application preference menu queues a refresh.
IntegrationSettings integration;
#ifdef DAC_HARNESS
std::map<std::wstring,std::wstring> integrationFixture;
#endif
void ReadIntegrationSettings();
#ifdef DAC_HARNESS
std::wstring IntegrationValue(const wchar_t* name) {
    auto found=integrationFixture.find(name);return found==integrationFixture.end()?L"":found->second;
}
void ReadIntegrationFixture() {
    integration.startupSetup=IntegrationValue(L"StartupPresentation")==L"setup";
    auto click=IntegrationValue(L"TrayClickAction");integration.trayAction=click==L"setup"?1:click==L"settings"?2:0;
    auto appearance=IntegrationValue(L"Appearance");integration.appearance=appearance==L"light"?1:appearance==L"dark"?2:0;
}
#endif
bool OpenSetupOnStart(bool missingConfig) {return missingConfig||integration.startupSetup;}
constexpr const wchar_t* savers[]={L"Bubbles.scr",L"Mystify.scr",L"Ribbons.scr",L"ssText3d.scr",L"PhotoScreensaver.scr",L"scrnsave.scr"};
struct Handle {
    HANDLE value{}; Handle()=default; explicit Handle(HANDLE h):value(h==INVALID_HANDLE_VALUE?nullptr:h) {}
    ~Handle() { if(value) CloseHandle(value); }
    Handle(const Handle&)=delete; Handle& operator=(const Handle&)=delete;
    operator HANDLE() const { return value; }
};
template<class T> struct Com {
    T* p{}; ~Com(){if(p)p->Release();} T** out(){return &p;} T* operator->()const{return p;}
};
std::wstring Wide(const std::string& s) {
    int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s.data(),static_cast<int>(s.size()),nullptr,0);
    std::wstring w(n,L'\0'); if(n) MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s.data(),static_cast<int>(s.size()),w.data(),n); return w;
}
std::string Utf8(const std::wstring& s) {
    int n=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,s.data(),static_cast<int>(s.size()),nullptr,0,nullptr,nullptr);
    std::string b(n,'\0'); if(n) WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,s.data(),static_cast<int>(s.size()),b.data(),n,nullptr,nullptr); return b;
}
std::wstring Extended(const std::wstring& path) {
    if(path.empty()||path.size()>32000||path.find(L'\0')!=std::wstring::npos) return {};
    if(path.starts_with(L"\\\\?\\")) return path;
    if(path.starts_with(L"\\\\")) return L"\\\\?\\UNC\\"+path.substr(2);
    if(path.size()<3||path[1]!=L':'||path[2]!=L'\\') return {};
    return L"\\\\?\\"+path;
}
bool ReadFileText(const std::wstring& path,std::string& text,DWORD& error,bool raw=false) {
    auto p=Extended(path); if(p.empty()) { error=ERROR_BAD_PATHNAME; return false; }
    Handle file(CreateFileW(p.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr));
    if(!file) { error=GetLastError(); return false; } LARGE_INTEGER size{};
    if(!GetFileSizeEx(file,&size)||size.QuadPart>262144||size.QuadPart<0) { error=ERROR_FILE_TOO_LARGE; return false; }
    text.resize(static_cast<size_t>(size.QuadPart)); DWORD got=0;
    if(!ReadFile(file,text.data(),static_cast<DWORD>(text.size()),&got,nullptr)||got!=text.size()) { error=ERROR_READ_FAULT; return false; }
    if(raw)return true;
    if(text.starts_with("\xef\xbb\xbf")) text.erase(0,3);
    if(!text.empty()&&Wide(text).empty()) { error=ERROR_NO_UNICODE_TRANSLATION; return false; }
    if(text.find('\0')!=std::string::npos) { error=ERROR_INVALID_DATA; return false; } return true;
}
// Same-directory replacement. Failure never commits live state. A failed final
// rename leaves the old file intact; stale private temp files are removed.
bool WriteFileText(const std::wstring& path,const std::string& text,DWORD& error) {
    auto p=Extended(path); if(p.empty()) { error=ERROR_BAD_PATHNAME; return false; }
    GUID nonce{};HRESULT created=CoCreateGuid(&nonce);
    if(FAILED(created)){error=ERROR_GEN_FAILURE;return false;}
    auto temp=p+L".tmp-"+Wide(Hex(std::string(reinterpret_cast<const char*>(&nonce),sizeof(nonce))));
    bool ok=false;
    { Handle f(CreateFileW(temp.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr));
      // Never follow/truncate a pre-existing temporary file or remove one we do not own.
      if(!f){error=GetLastError();return false;}
      DWORD wrote=0;
      ok=f && WriteFile(f,text.data(),static_cast<DWORD>(text.size()),&wrote,nullptr)&&wrote==text.size()&&FlushFileBuffers(f);
      if(!ok) error=GetLastError(); }
    if(ok) { ok=!!MoveFileExW(temp.c_str(),p.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH); if(!ok) error=GetLastError(); }
    if(!ok) DeleteFileW(temp.c_str()); return ok;
}
// A user-selected local path may not redirect through a share, mapped drive,
// alternate stream, junction or symlink. Inspect ancestors and the opened target.
bool LocalPath(const std::wstring& path,bool directory,bool fixedOnly=false) {
    if(path.size()<3||path.size()>32760||path[1]!=L':'||path[2]!=L'\\'||path.find(L':',2)!=std::wstring::npos||path.find(L'/')!=std::wstring::npos||path.find(L'\0')!=std::wstring::npos)return false;
    auto root=path.substr(0,3);auto drive=GetDriveTypeW(root.c_str());if(drive!=DRIVE_FIXED&&(fixedOnly||drive!=DRIVE_REMOVABLE))return false;
    std::wstring normalized=path;while(normalized.size()>3&&normalized.back()==L'\\')normalized.pop_back();
    for(size_t start=3;start<normalized.size();) {
        auto end=normalized.find(L'\\',start);if(end==std::wstring::npos)end=normalized.size();
        auto component=normalized.substr(start,end-start);if(component.empty()||component==L"."||component==L".."||component.back()==L'.'||component.back()==L' ')return false;
        auto attributes=GetFileAttributesW(Extended(normalized.substr(0,end)).c_str());
        if(attributes==INVALID_FILE_ATTRIBUTES||(attributes&FILE_ATTRIBUTE_REPARSE_POINT)||(end<normalized.size()&&!(attributes&FILE_ATTRIBUTE_DIRECTORY)))return false;
        start=end+1;
    }
    Handle target(CreateFileW(Extended(normalized).c_str(),FILE_READ_ATTRIBUTES,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS|FILE_FLAG_OPEN_REPARSE_POINT,nullptr));
    if(!target||GetFileType(target)!=FILE_TYPE_DISK)return false;
    FILE_ATTRIBUTE_TAG_INFO info{};if(!GetFileInformationByHandleEx(target,FileAttributeTagInfo,&info,sizeof(info))||(info.FileAttributes&FILE_ATTRIBUTE_REPARSE_POINT)||bool(info.FileAttributes&FILE_ATTRIBUTE_DIRECTORY)!=directory)return false;
    std::vector<wchar_t> finalPath(32768);DWORD length=GetFinalPathNameByHandleW(target,finalPath.data(),32768,FILE_NAME_NORMALIZED|VOLUME_NAME_DOS);
    if(!length||length>=32768)return false;std::wstring resolved(finalPath.data(),length);
    if(!resolved.starts_with(L"\\\\?\\")||resolved.size()<7||resolved[5]!=L':'||resolved[6]!=L'\\')return false;
    resolved.erase(0,4);return CompareStringOrdinal(normalized.c_str(),-1,resolved.c_str(),-1,TRUE)==CSTR_EQUAL;
}
std::wstring configDirectoryOverride;
bool SetConfigDirectory(const std::wstring& path) {
    auto normalized=path;std::replace(normalized.begin(),normalized.end(),L'/',L'\\');
    while(normalized.size()>3&&normalized.back()==L'\\')normalized.pop_back();
    if(normalized.size()<=3||!LocalPath(normalized,true,true))return false;configDirectoryOverride=std::move(normalized);return true;
}
std::wstring ConfigPath() {
    if(!configDirectoryOverride.empty())return LocalPath(configDirectoryOverride,true,true)?configDirectoryOverride+(configDirectoryOverride.back()==L'\\'?L"":L"\\")+L"settings-v1.ini":L"";
    PWSTR folder=nullptr;if(FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData,0,nullptr,&folder)))return {};
    std::wstring dir=std::wstring(folder)+L"\\DAC";CoTaskMemFree(folder);
    auto extended=Extended(dir);if(extended.empty())return {};
    if(!CreateDirectoryW(extended.c_str(),nullptr)&&GetLastError()!=ERROR_ALREADY_EXISTS)return {};
    DWORD attributes=GetFileAttributesW(extended.c_str());
    if(attributes==INVALID_FILE_ATTRIBUTES||!(attributes&FILE_ATTRIBUTE_DIRECTORY)||(attributes&FILE_ATTRIBUTE_REPARSE_POINT))return {};
    return dir+L"\\settings-v1.ini";
}
std::wstring SaverPath(int index) {
    if(index<0||index>5) return {}; std::vector<wchar_t> path(32768);
    // A 32-bit build may run on 64-bit Windows.
    // Sysnative selects the installed native savers without changing the
    // process/thread-wide WOW64 filesystem redirection state.
    BOOL wow64=FALSE; if(!IsWow64Process(GetCurrentProcess(),&wow64)) return {};
    if(wow64) { UINT n=GetWindowsDirectoryW(path.data(),32768);
        return n&&n<32768?std::wstring(path.data())+L"\\Sysnative\\"+savers[index]:L""; }
    UINT n=GetSystemDirectoryW(path.data(),32768);
    return n && n<32768?std::wstring(path.data())+L"\\"+savers[index]:L"";
}
bool Available(int index) { auto p=SaverPath(index); DWORD a=GetFileAttributesW(p.c_str()); return !p.empty()&&a!=INVALID_FILE_ATTRIBUTES&&!(a&FILE_ATTRIBUTE_DIRECTORY); }
struct Display { std::string id,alias; std::wstring label; HMONITOR handle{}; RECT rect{}; bool identified=false; };
std::vector<Display> displays;
Display spanningDisplay;
Controller controller;
std::wstring configPath;
std::atomic<HWND> ui{};
void ReadIntegrationSettings() {
#ifdef DAC_HARNESS
    ReadIntegrationFixture();
#else
    integration={};std::string text;DWORD error=0;IntegrationSettings next;
    if(!configPath.empty()&&ReadFileText(configPath+L".host",text,error)&&ParseIntegration(text,next))integration=next;
#endif
}
bool WriteIntegrationSettings(const IntegrationSettings& next,DWORD& error) {
    IntegrationSettings checked;if(!ParseIntegration(SerializeIntegration(next),checked)){error=ERROR_INVALID_DATA;return false;}
#ifdef DAC_HARNESS
    integrationFixture={{L"StartupPresentation",next.startupSetup?L"setup":L"tray"},
        {L"TrayClickAction",next.trayAction==1?L"setup":next.trayAction==2?L"settings":L"toggle"},
        {L"Appearance",next.appearance==1?L"light":next.appearance==2?L"dark":L"system"}};
    integration=next;return true;
#else
    if(configPath.empty()){error=ERROR_PATH_NOT_FOUND;return false;}
    if(!WriteFileText(configPath+L".host",SerializeIntegration(next),error))return false;
    integration=next;return true;
#endif
}
std::wstring LoginCommand() {
    std::vector<wchar_t> executable(32768);DWORD size=GetModuleFileNameW(nullptr,executable.data(),32768);
    if(!size||size>=32768)return {};auto command=L"\""+std::wstring(executable.data(),size)+L"\" --background";
    if(!configDirectoryOverride.empty())command+=L" --config-directory \""+configDirectoryOverride+L"\"";
    return command;
}
#ifdef DAC_HARNESS
int loginStartupFixture=0;
#endif
// 0 absent, 1 this exact executable, 2 another value, -1 query failed.
int LoginStartupState() {
#ifdef DAC_HARNESS
    return loginStartupFixture;
#else
    std::vector<wchar_t> value(32768);const auto capacityBytes=static_cast<DWORD>(value.size()*sizeof(wchar_t));
    HKEY key=nullptr;auto opened=RegOpenKeyExW(HKEY_CURRENT_USER,L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",0,KEY_QUERY_VALUE,&key);
    if(opened==ERROR_FILE_NOT_FOUND)return 0;if(opened!=ERROR_SUCCESS)return -1;
    DWORD bytes=capacityBytes,type=0;
    auto result=RegQueryValueExW(key,L"DAC",nullptr,&type,reinterpret_cast<BYTE*>(value.data()),&bytes);RegCloseKey(key);
    if(result==ERROR_FILE_NOT_FOUND)return 0;
    if(result!=ERROR_SUCCESS||type!=REG_SZ||bytes<sizeof(wchar_t)||bytes>capacityBytes||bytes%sizeof(wchar_t))return -1;
    if(value[bytes/sizeof(wchar_t)-1]!=L'\0')return -1;
    return std::wstring(value.data())==LoginCommand()?1:2;
#endif
}
bool SetLoginStartup(bool enabled,DWORD& error) {
#ifdef DAC_HARNESS
    if(loginStartupFixture<0||loginStartupFixture==2){error=ERROR_ALREADY_EXISTS;return false;}
    loginStartupFixture=enabled?1:0;return true;
#else
    // Never overwrite/remove a Run value owned by a different installation.
    auto state=LoginStartupState();if(state<0||state==2){error=ERROR_ALREADY_EXISTS;return false;}
    auto command=LoginCommand();if(command.empty()){error=ERROR_BAD_PATHNAME;return false;}
    HKEY key=nullptr;auto result=RegCreateKeyExW(HKEY_CURRENT_USER,L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",0,nullptr,0,KEY_QUERY_VALUE|KEY_SET_VALUE,nullptr,&key,nullptr);
    if(result!=ERROR_SUCCESS){error=result;return false;}
    result=enabled?RegSetValueExW(key,L"DAC",0,REG_SZ,reinterpret_cast<const BYTE*>(command.c_str()),static_cast<DWORD>((command.size()+1)*sizeof(wchar_t))):RegDeleteValueW(key,L"DAC");
    RegCloseKey(key);if(result!=ERROR_SUCCESS&&(enabled||result!=ERROR_FILE_NOT_FOUND)){error=result;return false;}
    error=0;return result==ERROR_SUCCESS||(!enabled&&result==ERROR_FILE_NOT_FOUND);
#endif
}
// Build-generated from the pinned Fujin dependency.
#include "generated/fujin.hpp"
struct UiTheme {
    fujin::Palette colors=fujin::dark;
    bool highContrast=false,dark=true;
    HBRUSH base{},surface{};
    HICON smallIcon{},largeIcon{},activeIcon{};
} theme;
#ifdef DAC_HARNESS
int testTheme=-1; // 0 light, 1 dark, 2 high contrast; no Windows preference writes.
#endif
void FillColor(HDC dc,const RECT& rect,COLORREF color) {SetDCBrushColor(dc,color);FillRect(dc,&rect,static_cast<HBRUSH>(GetStockObject(DC_BRUSH)));}
void FrameColor(HDC dc,const RECT& rect,COLORREF color) {SetDCBrushColor(dc,color);FrameRect(dc,&rect,static_cast<HBRUSH>(GetStockObject(DC_BRUSH)));}
HICON DacIcon(int size,bool active) {
    Gdiplus::Bitmap image(size,size,PixelFormat32bppARGB);Gdiplus::Graphics g(&image);
    g.Clear(Gdiplus::Color(0,0,0,0));g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);g.ScaleTransform(size/32.f,size/32.f);
    auto c=theme.colors.accent;Gdiplus::SolidBrush accent(Gdiplus::Color(255,GetRValue(c),GetGValue(c),GetBValue(c)));
    auto ink=theme.colors.onAccent;Gdiplus::Pen outline(Gdiplus::Color(255,GetRValue(ink),GetGValue(ink),GetBValue(ink)),2.f);
    g.FillRectangle(&accent,2.f,4.f,27.f,19.f);g.DrawRectangle(&outline,2.f,4.f,27.f,19.f);
    g.DrawLine(&outline,12.f,27.f,21.f,27.f);g.DrawLine(&outline,16.f,23.f,16.f,27.f);
    Gdiplus::PointF shield[]={{16,8},{23,11},{22,17},{16,21},{10,17},{9,11}};
    auto base=theme.colors.base;Gdiplus::SolidBrush face(Gdiplus::Color(255,GetRValue(base),GetGValue(base),GetBValue(base)));g.FillPolygon(&face,shield,6);g.DrawPolygon(&outline,shield,6);
    if(active){g.DrawLine(&outline,12.f,14.f,15.f,17.f);g.DrawLine(&outline,15.f,17.f,20.f,12.f);}
    else g.DrawLine(&outline,16.f,12.f,16.f,17.f);
    HICON icon{};if(image.GetHICON(&icon)!=Gdiplus::Ok)return nullptr;return icon;
}
void ReadUiTheme() {
    HIGHCONTRASTW hc{};hc.cbSize=sizeof(hc);theme.highContrast=SystemParametersInfoW(SPI_GETHIGHCONTRAST,sizeof(hc),&hc,0)&&(hc.dwFlags&HCF_HIGHCONTRASTON);
    DWORD light=0,bytes=sizeof(light);RegGetValueW(HKEY_CURRENT_USER,L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",L"AppsUseLightTheme",RRF_RT_REG_DWORD,nullptr,&light,&bytes);theme.dark=!light;
#ifdef DAC_HARNESS
    if(testTheme>=0){theme.highContrast=testTheme==2;theme.dark=testTheme==1;}
#endif
    if(integration.appearance)theme.dark=integration.appearance==2;
    theme.colors=theme.dark?fujin::dark:fujin::light;
    if(theme.highContrast){auto& p=theme.colors;p.base=GetSysColor(COLOR_WINDOW);p.surface=p.base;p.elevated=GetSysColor(COLOR_BTNFACE);p.text=GetSysColor(COLOR_WINDOWTEXT);p.secondary=GetSysColor(COLOR_GRAYTEXT);p.onAccent=GetSysColor(COLOR_HIGHLIGHTTEXT);p.border=p.strong=p.text;p.accent=p.hover=p.active=GetSysColor(COLOR_HIGHLIGHT);}
    if(theme.base)DeleteObject(theme.base);if(theme.surface)DeleteObject(theme.surface);
    theme.base=CreateSolidBrush(theme.colors.base);theme.surface=CreateSolidBrush(theme.colors.surface);
}
HFONT SettingsFont(int dpi,int minimumSize=0) {
    NONCLIENTMETRICSW metrics{};metrics.cbSize=sizeof(metrics);
    if(!SystemParametersInfoForDpi(SPI_GETNONCLIENTMETRICS,sizeof(metrics),&metrics,0,dpi))return nullptr;
    if(!theme.highContrast){metrics.lfMessageFont.lfHeight=-MulDiv(fujin::fontSize,dpi,96);metrics.lfMessageFont.lfWeight=FW_NORMAL;wcscpy_s(metrics.lfMessageFont.lfFaceName,fujin::fontFamily);}
    if(minimumSize)metrics.lfMessageFont.lfHeight=-std::max<LONG>(abs(metrics.lfMessageFont.lfHeight),MulDiv(minimumSize,dpi,96));
    return CreateFontIndirectW(&metrics.lfMessageFont);
}
void ThemeChrome(HWND w) {
    BOOL dark=theme.dark&&!theme.highContrast;DwmSetWindowAttribute(w,DWMWA_USE_IMMERSIVE_DARK_MODE,&dark,sizeof(dark));
    DWORD corner=DWMWCP_DEFAULT;if constexpr(fujin::radius==0)corner=DWMWCP_DONOTROUND;DwmSetWindowAttribute(w,DWMWA_WINDOW_CORNER_PREFERENCE,&corner,sizeof(corner));
    COLORREF bg=theme.highContrast?DWMWA_COLOR_DEFAULT:theme.colors.chrome,fg=theme.highContrast?DWMWA_COLOR_DEFAULT:theme.colors.chromeText;
    DwmSetWindowAttribute(w,DWMWA_CAPTION_COLOR,&bg,sizeof(bg));DwmSetWindowAttribute(w,DWMWA_TEXT_COLOR,&fg,sizeof(fg));
    SendMessageW(w,WM_SETICON,ICON_SMALL,reinterpret_cast<LPARAM>(theme.smallIcon));SendMessageW(w,WM_SETICON,ICON_BIG,reinterpret_cast<LPARAM>(theme.largeIcon));
}
void PaintThemedControl(HWND w,HDC dc,int kind) {
    int saved=SaveDC(dc);RECT r{};GetClientRect(w,&r);auto& p=theme.colors;bool enabled=IsWindowEnabled(w);
    auto font=reinterpret_cast<HFONT>(SendMessageW(w,WM_GETFONT,0,0));if(font)SelectObject(dc,font);SetBkMode(dc,TRANSPARENT);
    int dpi=GetDpiForWindow(w),pad=MulDiv(fujin::spacing/2,dpi,96);bool focused=GetFocus()==w;bool hideFocus=SendMessageW(w,WM_QUERYUISTATE,0,0)&UISF_HIDEFOCUS;
    auto state=SendMessageW(w,BM_GETSTATE,0,0);bool down=kind!=3&&(state&BST_PUSHED);bool primary=GetDlgCtrlID(w)==130||GetDlgCtrlID(w)==220||GetDlgCtrlID(w)==406;
    bool hot=GetPropW(w,L"DacHover")!=nullptr;COLORREF fg=enabled?p.text:p.secondary;
    std::wstring text;int length=GetWindowTextLengthW(w);text.resize(length+1);GetWindowTextW(w,text.data(),length+1);text.resize(length);
    RECT label=r;UINT flags=DT_SINGLELINE|DT_VCENTER|DT_END_ELLIPSIS;
    if(SendMessageW(w,WM_QUERYUISTATE,0,0)&UISF_HIDEACCEL)flags|=DT_HIDEPREFIX;
    if(kind==1){
        FillColor(dc,r,p.base);int side=MulDiv(fujin::spacing,dpi,96);RECT box{0,(r.bottom-side)/2,side,(r.bottom+side)/2};bool checked=SendMessageW(w,BM_GETCHECK,0,0)==BST_CHECKED;
        FillColor(dc,box,checked&&enabled?p.accent:p.surface);FrameColor(dc,box,hot&&enabled?p.accent:p.strong);
        if(checked){HPEN pen=CreatePen(PS_SOLID,std::max(1,MulDiv(2,dpi,96)),enabled?p.onAccent:p.secondary);auto old=SelectObject(dc,pen);MoveToEx(dc,box.left+side/4,box.top+side/2,nullptr);LineTo(dc,box.left+side*7/16,box.top+side*3/4);LineTo(dc,box.right-side/5,box.top+side/4);SelectObject(dc,old);DeleteObject(pen);}
        label.left=side+pad;
    }else{
        COLORREF bg=primary&&enabled?(down?p.active:hot?p.hover:p.accent):(down?p.surface:hot?p.elevated:p.surface);
        FillColor(dc,r,bg);FrameColor(dc,r,focused||hot?p.accent:p.border);if(primary&&enabled)fg=p.onAccent;label.left+=pad;label.right-=pad;
        if(kind==3){int arrow=MulDiv(24,dpi,96);label.right-=arrow;int x=r.right-arrow/2,y=r.bottom/2;POINT points[]={{x-4,y-2},{x+4,y-2},{x,y+3}};SetDCBrushColor(dc,fg);SelectObject(dc,GetStockObject(DC_BRUSH));SelectObject(dc,GetStockObject(NULL_PEN));Polygon(dc,points,3);flags|=DT_NOPREFIX;}
        else flags|=DT_CENTER;
    }
    SetTextColor(dc,fg);DrawTextW(dc,text.c_str(),static_cast<int>(text.size()),&label,flags);
    if(focused&&!hideFocus){RECT focus=kind==1?label:r;InflateRect(&focus,-3,-3);SetTextColor(dc,p.text);SetBkColor(dc,p.base);DrawFocusRect(dc,&focus);}
    RestoreDC(dc,saved);
}
LRESULT CALLBACK ThemeControlProc(HWND w,UINT msg,WPARAM wp,LPARAM lp) {
    auto original=reinterpret_cast<WNDPROC>(GetPropW(w,L"DacOriginalProc"));int kind=static_cast<int>(reinterpret_cast<INT_PTR>(GetPropW(w,L"DacKind")));
    if(msg==WM_NCDESTROY){SetWindowLongPtrW(w,GWLP_WNDPROC,reinterpret_cast<LONG_PTR>(original));RemovePropW(w,L"DacHover");RemovePropW(w,L"DacKind");RemovePropW(w,L"DacOriginalProc");return CallWindowProcW(original,w,msg,wp,lp);}
    if(!theme.highContrast&&(msg==WM_PAINT||msg==WM_PRINTCLIENT)){
        PAINTSTRUCT ps{};HDC dc=msg==WM_PAINT?BeginPaint(w,&ps):reinterpret_cast<HDC>(wp);PaintThemedControl(w,dc,static_cast<int>(kind));if(msg==WM_PAINT)EndPaint(w,&ps);return 0;
    }
    if(msg==WM_MOUSEMOVE&&!GetPropW(w,L"DacHover")){SetPropW(w,L"DacHover",reinterpret_cast<HANDLE>(1));TRACKMOUSEEVENT track{sizeof(track),TME_LEAVE,w,0};TrackMouseEvent(&track);InvalidateRect(w,nullptr,FALSE);}
    if(msg==WM_MOUSELEAVE){RemovePropW(w,L"DacHover");InvalidateRect(w,nullptr,FALSE);}
    auto result=CallWindowProcW(original,w,msg,wp,lp);
    if(msg==BM_SETCHECK||msg==BM_SETSTATE||msg==WM_ENABLE||msg==WM_SETFOCUS||msg==WM_KILLFOCUS||msg==WM_UPDATEUISTATE||msg==CB_SETCURSEL||msg==WM_SETTEXT)InvalidateRect(w,nullptr,FALSE);
    return result;
}
void ThemeControl(HWND w,const wchar_t* cls) {
    int kind=0;if(wcscmp(cls,L"BUTTON")==0)kind=(GetWindowLongPtrW(w,GWL_STYLE)&BS_TYPEMASK)==BS_AUTOCHECKBOX?1:2;
    else if(wcscmp(cls,L"COMBOBOX")==0)kind=3;
    if(!w||!kind)return;
    auto original=GetWindowLongPtrW(w,GWLP_WNDPROC);
    if(!SetPropW(w,L"DacOriginalProc",reinterpret_cast<HANDLE>(original)))return;
    if(!SetPropW(w,L"DacKind",reinterpret_cast<HANDLE>(static_cast<INT_PTR>(kind)))){RemovePropW(w,L"DacOriginalProc");return;}
    SetLastError(0);if(!SetWindowLongPtrW(w,GWLP_WNDPROC,reinterpret_cast<LONG_PTR>(ThemeControlProc))&&GetLastError()){RemovePropW(w,L"DacOriginalProc");RemovePropW(w,L"DacKind");}
}
bool ThemeMessage(HWND w,UINT msg,WPARAM wp,LPARAM lp,LRESULT& result) {
    result=0;
    if(msg==WM_ERASEBKGND||msg==WM_PRINTCLIENT){RECT r{};GetClientRect(w,&r);FillRect(reinterpret_cast<HDC>(wp),&r,theme.base);result=1;return true;}
    if(msg==WM_CTLCOLORSTATIC||msg==WM_CTLCOLOREDIT||msg==WM_CTLCOLORLISTBOX||msg==WM_CTLCOLORBTN){
        if(theme.highContrast)return false;HDC dc=reinterpret_cast<HDC>(wp);bool surface=msg==WM_CTLCOLOREDIT||msg==WM_CTLCOLORLISTBOX;
        SetTextColor(dc,IsWindowEnabled(reinterpret_cast<HWND>(lp))?theme.colors.text:theme.colors.secondary);SetBkColor(dc,surface?theme.colors.surface:theme.colors.base);result=reinterpret_cast<LRESULT>(surface?theme.surface:theme.base);return true;
    }
    if(msg==WM_DRAWITEM){auto d=reinterpret_cast<DRAWITEMSTRUCT*>(lp);if(d->CtlType!=ODT_COMBOBOX)return false;
        int saved=SaveDC(d->hDC);bool selected=d->itemState&ODS_SELECTED;auto& p=theme.colors;FillColor(d->hDC,d->rcItem,selected?p.accent:p.surface);SetBkMode(d->hDC,TRANSPARENT);SetTextColor(d->hDC,selected?p.onAccent:p.text);
        auto font=SendMessageW(d->hwndItem,WM_GETFONT,0,0);if(font)SelectObject(d->hDC,reinterpret_cast<HFONT>(font));
        if(d->itemID!=static_cast<UINT>(-1)){int n=static_cast<int>(SendMessageW(d->hwndItem,CB_GETLBTEXTLEN,d->itemID,0));if(n>=0&&n<32768){std::wstring text(n+1,L'\0');SendMessageW(d->hwndItem,CB_GETLBTEXT,d->itemID,reinterpret_cast<LPARAM>(text.data()));RECT r=d->rcItem;r.left+=MulDiv(8,GetDpiForWindow(w),96);DrawTextW(d->hDC,text.c_str(),n,&r,DT_SINGLELINE|DT_VCENTER|DT_NOPREFIX|DT_END_ELLIPSIS);}}
        if((d->itemState&ODS_FOCUS)&&!(d->itemState&ODS_NOFOCUSRECT))DrawFocusRect(d->hDC,&d->rcItem);RestoreDC(d->hDC,saved);result=TRUE;return true;
    }
    if(msg==WM_MEASUREITEM){auto m=reinterpret_cast<MEASUREITEMSTRUCT*>(lp);if(m->CtlType==ODT_COMBOBOX){m->itemHeight=MulDiv(24,GetDpiForWindow(w),96);result=TRUE;return true;}}
    return false;
}

HWND editor{},workspace{},setupWindow{};
HFONT editorFont{},workspaceFont{},setupFont{};
HWND options{};HFONT optionsFont{};ULONG_PTR imagingToken{};
std::wstring hotkeyStatus;
HANDLE uiThread{},safetyThread{},mediaThread{},stopEvent{},readyEvent{},safetyReady{},singleton{};
std::atomic<bool> ready{},safetyOK{},unloading{},debug{},startupComplete{};
// The process supervisor never waits on UI/COM/file/driver workers to enforce exit.
std::atomic<Time> shutdownRequestedAt{};
std::atomic<bool> backgroundStart{};
void BeginShutdownDeadline(){Time expected=0;shutdownRequestedAt.compare_exchange_strong(expected,GetTickCount64());}
UINT taskbar{}; HPOWERNOTIFY powerNotification{};
bool trayPresent=false,locked=false,suspended=false,displayOff=false,topologyPending=false;
Time nextPoll=0,nextTray=0; DWORD lastInputStamp=0,rawStamp=0;
std::mutex observationLock;
struct AdapterSnapshot {Time at=0,inputAt=0;bool controllerAvailable=false,connected=false,active=false;PowerSource power=PowerSource::Unknown;};
AdapterSnapshot adapters;
Time consumedControllerInput=0;
void ConsumeAdapters(const AdapterSnapshot& sample,Time now){
    bool fresh=sample.at&&now>=sample.at&&now-sample.at<=1000;
    controller.activityStale=controller.PolicyNow().controllerInput&&!fresh;
    controller.powerSource=fresh?sample.power:PowerSource::Unknown;
    if(fresh&&controller.PolicyNow().controllerInput&&sample.inputAt>consumedControllerInput){controller.Input(now,controller.foreground.monitors,controller.foreground.valid&&!controller.foreground.monitors.empty(),sample.inputAt);consumedControllerInput=sample.inputAt;}
}
Media observation; Config mediaConfig; std::vector<Display> mediaDisplays; uint64_t observationEpoch=0;
std::mutex registryLock; // Only handle publication/removal; NEVER held across launch/media queries.
std::map<HANDLE,HWND> ownedJobs;
std::set<HWND> ownedWindows;
std::atomic<unsigned> diagnostics{};
struct DiagnosticEvent {Time at;std::string alias,event;DWORD code;};
std::mutex diagnosticLock;std::deque<DiagnosticEvent> diagnosticEvents;
void Record(const std::string& alias,const std::string& event,DWORD code=0){std::lock_guard lock(diagnosticLock);if(diagnosticEvents.size()>=128)diagnosticEvents.pop_front();diagnosticEvents.push_back({GetTickCount64(),alias.substr(0,32),event.substr(0,96),code});}
// Aggregate lifetime counters saturate, including histogram overflow bins.
// They are observations from different events, never an event trace.
void Count(std::atomic<Time>& value,Time amount=1){auto old=value.load(std::memory_order_relaxed);while(!value.compare_exchange_weak(old,amount>UINT64_MAX-old?UINT64_MAX:old+amount,std::memory_order_relaxed)) {}}
struct TimingBins {std::array<std::atomic<Time>,8> bins{};};
void Sample(TimingBins& histogram,Time value){
    constexpr std::array<Time,8> upper={1,4,16,64,256,1024,4096,UINT64_MAX};
    for(size_t bin=0;bin<histogram.bins.size();++bin){
        if(value<=upper[bin]){Count(histogram.bins[bin]);return;}
    }
}
enum class ResetCause {Other,Session,Power,Tray,Topology};
struct WakeTiming {
    std::atomic<Time> inputs{},accepted{},invalidTime{},boundaryRejected{},unattributed{},stickyRejected{},focusUnavailable{},readFailures{},
        inhibited{},queueMaxMs{},heartbeatMaxMs{},inputMaxUs{},reconcileMaxUs{},timerMaxUs{},fileMaxUs{},trayMaxUs{},staleResults{},fileFailures{},trayCalls{};
    std::atomic<Time> receipts{},rawCalls{},readSuccess{},rawFailure{},fallbackReadFailure{},unsupported{},validTime{},directMetadata{},nestedMetadata{},
        pointerAbsent{},pointerUnique{},pointerAmbiguous{},foregroundFresh{},foregroundUnavailable{},topologyRejected{},timestampBoundary{},
        reconcileEntries{},newlyCancelled{},newlyCancelledPower{},hideRequests{},preVisible{},preHidden{},preInvalid{},postVisible{},postHidden{},postInvalid{},
        heartbeatSamples{},paintSamples{},paintMaxUs{},paintPixelsMax{},blackPaints{},clockPaints{},constellationPaints{},coverageEmpty{},coveragePartial{},coverageFull{};
    std::array<std::atomic<Time>,5> resets{};
    std::array<std::atomic<Time>,8> coverage{}; // 0,1,2,3,4-7,8-15,16-31,32+ active nodes
    TimingBins rawUs,dispatchUs,paintUs,timerUs,heartbeatMs,queueMs;
} wakeTiming;
void Maximum(std::atomic<Time>& value,Time sample){auto old=value.load();while(old<sample&&!value.compare_exchange_weak(old,sample)) {}}
Time Micros(){return static_cast<Time>(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());}
struct Measure {
    std::atomic<Time>& maximum;TimingBins* histogram;Time began=Micros();
    explicit Measure(std::atomic<Time>& value,TimingBins* bins=nullptr):maximum(value),histogram(bins){}
    ~Measure(){auto elapsed=Micros()-began;Maximum(maximum,elapsed);if(histogram)Sample(*histogram,elapsed);}
};
void ObserveCoverage(){
    size_t active=0;for(auto& [id,n]:controller.nodes){(void)id;if(Running(n.state))++active;}
    size_t bin=active<=3?active:active<8?4:active<16?5:active<32?6:7;Count(wakeTiming.coverage[bin]);
    if(!active)Count(wakeTiming.coverageEmpty);else if(active==controller.nodes.size())Count(wakeTiming.coverageFull);else Count(wakeTiming.coveragePartial);
}
Time heartbeatAt=0;
void InvalidateFaults();
bool CachedFault(const std::string& id);
bool FaultResetPending(const std::string& id);
void RequestFaultReset(const std::string& id);
void QueueNotice(const wchar_t* text);
void Maintain(Time now);
std::map<std::string,Reason> lastReasons;
const char* ReasonText(Reason r){switch(r){case Reason::Disabled:return "Disabled";case Reason::Session:return "Session or tray blocked";case Reason::Paused:return "Paused for this session";case Reason::Snoozed:return "Automatic activation snoozed";case Reason::ManualOnly:return "Manual only";case Reason::App:return "Foreground application rule";case Reason::Fullscreen:return "Fullscreen foreground application";case Reason::Stale:return "Observation unavailable or stale";case Reason::Media:return "Media activity (process/window heuristic)";case Reason::Idle:return "Waiting for idle";case Reason::Dim:return "Software dim warning";case Reason::Manual:return "Manual presentation";case Reason::Sticky:return "Sticky presentation";case Reason::Presentation:return "Presentation active";case Reason::Fallback:return "Presentation failed; black fallback";case Reason::HardwareFault:return "Hardware fault quarantined";}return "Unknown";}
std::wstring ExplanationText(const std::string& id) {
    Media m;{std::lock_guard lock(observationLock);m=observation;}auto e=controller.Explain(id,m,GetTickCount64());auto text=Wide(ReasonText(e.primary));
    if(e.kind!=DeadlineKind::None){auto label=e.kind==DeadlineKind::Activation?L" — activation in ":e.kind==DeadlineKind::Presentation?L" — presentation in ":e.kind==DeadlineKind::Black?L" — black in ":L" — hardware request in ";text+=label+std::to_wstring((e.remaining+999)/1000)+L" s";}
    else if(e.primary==Reason::Snoozed)text+=L" — resumes in "+std::to_wstring((e.remaining+999)/1000)+L" s";
    if(e.flags&(1u<<static_cast<unsigned>(Reason::HardwareFault)))text+=L"; hardware fault retained";
    return text;
}
#ifdef DAC_HARNESS
int initFault=0;
std::atomic<DWORD> launchDelay{};
bool injectedObservations=false;
std::atomic<int> trayFailures{};int configurationFailures=0;
int legacyStartupOverride=-1,setupCloseResponse=IDNO;
#endif
void Note(const wchar_t* event,DWORD code=0) {
    Record("host",Utf8(event),wcscmp(event,L"owned process started")==0?0:code);
#ifdef DAC_HARNESS
    wprintf(L"%llu %ls code=%lu\n",GetTickCount64(),event,code); fflush(stdout);
#else
    if(debug && diagnostics.fetch_add(1)<1000) { auto message=std::wstring(L"DAC: ")+event+L" code="+std::to_wstring(code)+L"\n"; OutputDebugStringW(message.c_str()); }
#endif
}
void Notice(const wchar_t* text) {QueueNotice(text);}
BOOL CALLBACK EnumMonitor(HMONITOR m,HDC,LPRECT r,LPARAM data) {
    auto list=reinterpret_cast<std::vector<Display>*>(data); MONITORINFOEXW info{}; info.cbSize=sizeof(info);
    if(list->size()>=32||!GetMonitorInfoW(m,&info)) return TRUE;
    Display d; d.handle=m; d.rect=*r; d.alias=Utf8(info.szDevice); d.label=info.szDevice;
    list->push_back(d); return TRUE;
}
std::vector<Display> Catalog() {
    std::vector<Display> list; EnumDisplayMonitors(nullptr,nullptr,EnumMonitor,reinterpret_cast<LPARAM>(&list));
    // One logical source with multiple physical targets is a clone group. It is
    // deliberately disabled: independent physical presentation is impossible.
    std::map<std::string,std::vector<std::pair<std::string,std::wstring>>> identities;
    for(int retry=0;retry<3;++retry) {
        UINT32 pc=0,mc=0; if(GetDisplayConfigBufferSizes(QDC_ONLY_ACTIVE_PATHS,&pc,&mc)!=ERROR_SUCCESS||pc>256||mc>1024) break;
        std::vector<DISPLAYCONFIG_PATH_INFO> paths(pc); std::vector<DISPLAYCONFIG_MODE_INFO> modes(mc);
        LONG status=QueryDisplayConfig(QDC_ONLY_ACTIVE_PATHS,&pc,paths.data(),&mc,modes.data(),nullptr);
        if(status==ERROR_INSUFFICIENT_BUFFER) continue; if(status!=ERROR_SUCCESS) break;
        for(UINT32 i=0;i<pc;++i) {
            auto& p=paths[i]; DISPLAYCONFIG_SOURCE_DEVICE_NAME source{};
            source.header={DISPLAYCONFIG_DEVICE_INFO_GET_SOURCE_NAME,sizeof(source),p.sourceInfo.adapterId,p.sourceInfo.id};
            DISPLAYCONFIG_TARGET_DEVICE_NAME target{};
            target.header={DISPLAYCONFIG_DEVICE_INFO_GET_TARGET_NAME,sizeof(target),p.targetInfo.adapterId,p.targetInfo.id};
            if(DisplayConfigGetDeviceInfo(&source.header)==ERROR_SUCCESS&&DisplayConfigGetDeviceInfo(&target.header)==ERROR_SUCCESS&&target.monitorDevicePath[0])
                identities[Utf8(source.viewGdiDeviceName)].push_back({Canonical(Utf8(target.monitorDevicePath)),target.monitorFriendlyDeviceName});
        } break;
    }
    std::map<std::string,int> count;
    for(auto& [alias,ids]:identities) { (void)alias; for(auto& [id,label]:ids) { (void)label; ++count[id]; } }
    for(auto& d:list) {
        auto& ids=identities[d.alias];
        if(ids.size()==1&&count[ids[0].first]==1) { d.id=ids[0].first; d.label+=L" — "+ids[0].second; d.identified=true; }
        else { d.id="unidentified:"+d.alias; d.label+=L" (unidentified or cloned; disabled)"; }
    } return list;
}

// Hardware control is isolated from UI/media/emergency threads. No DDC reads
// occur during catalog/startup. Only an opted-in exact unique target is touched.
std::wstring PowerMarker(const std::string& id) {
    uint64_t hash=1469598103934665603ULL;for(unsigned char c:id){hash^=c;hash*=1099511628211ULL;}
    return configPath+L".power-"+std::to_wstring(hash)+L".pending";
}
// A failed query is not proof of absence. Both UI cache and final worker
// quarantine checks retain faults on access/I/O errors.
bool MarkerPending(const std::wstring& path){DWORD attributes=GetFileAttributesW(Extended(path).c_str());if(attributes!=INVALID_FILE_ATTRIBUTES)return true;DWORD error=GetLastError();return error!=ERROR_FILE_NOT_FOUND&&error!=ERROR_PATH_NOT_FOUND;}
bool PowerPending(const std::string& id) {return MarkerPending(PowerMarker(id));}
std::wstring PowerCancelName(const std::string& token){return objectScope+L"-Power-Cancel-"+Wide(token);}
bool PowerTicket(const std::string& marker,const std::string& id,const std::string& token,bool wake) {
    auto prefix=id+"\n"+token+"\n";return wake?(marker==prefix+"changing"||marker==prefix+"off"):(marker==prefix+"probe");
}
int PowerChild(const std::string& id,bool wake,const std::string& token) {
    configPath=ConfigPath();if(configPath.empty())return 10;
    if(token.size()!=32||token.find_first_not_of("0123456789abcdef")!=std::string::npos)return 10;
    Handle cancelled(OpenEventW(SYNCHRONIZE,FALSE,PowerCancelName(token).c_str()));if(!cancelled)return 10;
    auto markerPath=PowerMarker(id);auto lockName=objectScope+L"-DDC-"+markerPath.substr(configPath.size()+7);
    Handle mutex(CreateMutexW(nullptr,FALSE,lockName.c_str()));if(!mutex)return 10;
    DWORD acquired=WaitForSingleObject(mutex,0);if(acquired!=WAIT_OBJECT_0&&acquired!=WAIT_ABANDONED)return 10;
    struct Unlock {HANDLE mutex;~Unlock(){ReleaseMutex(mutex);}}unlock{mutex};
    std::string marker,text,error;DWORD code=0;
    if(!ReadFileText(markerPath,marker,code)||!PowerTicket(marker,id,token,wake))return 10;
    if(!wake&&WaitForSingleObject(cancelled,0)!=WAIT_TIMEOUT)return 10;
    if(!wake) {Config c;if(!ReadFileText(configPath,text,code)||!Parse(text,c,error)||!c.monitors.contains(id)||!c.monitors[id].hardware)return 10;}
    auto list=Catalog();auto found=std::find_if(list.begin(),list.end(),[&](const Display& d){return d.identified&&d.id==id;});
    if(found==list.end())return 10;
    DWORD count=0;if(!GetNumberOfPhysicalMonitorsFromHMONITOR(found->handle,&count)||count!=1)return 11;
    PHYSICAL_MONITOR physical{};if(!GetPhysicalMonitorsFromHMONITOR(found->handle,1,&physical))return 11;
    int result=11;
    if(wake) {if(WriteFileText(markerPath,id+"\n"+token+"\nwaking",code))result=SetVCPFeature(physical.hPhysicalMonitor,0xd6,0x01)?0:12;}
    else {
        MC_VCP_CODE_TYPE type{};DWORD current=0,maximum=0;
        // Avoid capabilities-string reads: some drivers/monitors mishandle them.
        // A D6 read does not prove that every power value is supported.
        if(GetVCPFeatureAndVCPFeatureReply(physical.hPhysicalMonitor,0xd6,&type,&current,&maximum)) {
            if(current!=0x01||WaitForSingleObject(cancelled,0)!=WAIT_TIMEOUT)result=10; // never take ownership of an already-off display
            else if(WriteFileText(markerPath,id+"\n"+token+"\nchanging",code)) {
                result=SetVCPFeature(physical.hPhysicalMonitor,0xd6,0x04)?0:12;
                if(result==0&&!WriteFileText(markerPath,id+"\n"+token+"\noff",code))result=12;
            }
        }
    }
    DestroyPhysicalMonitors(1,&physical);return result;
}
// One loaded helper owns the entire cycle, including wake during application shutdown.
// The nonce-bound acknowledgement, not exit code alone, establishes completion.
template<class Cancel,class Action> int PowerCycle(const std::string& id,const std::string& token,Cancel cancelled,Action action) {
    DWORD error=0;std::string marker;const auto path=PowerMarker(id);const auto prefix=id+"\n"+token+"\n";
    if(!ReadFileText(path,marker,error)||!PowerTicket(marker,id,token,false))return 10;
    int result=action(false);
    bool readable=ReadFileText(path,marker,error);bool changed=result==0||(readable&&PowerTicket(marker,id,token,true));
    if(changed){if(result==0)while(!cancelled())Sleep(50);
        if(action(true)==0&&WriteFileText(path,prefix+"awake",error))return 0;
        WriteFileText(path,prefix+"fault",error);return 12;
    }
    if(readable&&PowerTicket(marker,id,token,false)&&(result==10||result==11)) {WriteFileText(path,prefix+"untouched",error);return result;}
    return 13;
}
int PowerGuardian(const std::string& id,const std::string& token,HANDLE parent=nullptr) {
    configPath=ConfigPath();Handle cancelled(OpenEventW(SYNCHRONIZE,FALSE,PowerCancelName(token).c_str()));if(!cancelled)return 10;
    return PowerCycle(id,token,[&]{return WaitForSingleObject(cancelled,0)!=WAIT_TIMEOUT||(parent&&WaitForSingleObject(parent,0)!=WAIT_TIMEOUT);},[&](bool wake){return PowerChild(id,wake,token);});
}
struct PowerTask {
    std::string id,token;Time generation=0;std::atomic<bool> cancel{},done{};
    std::atomic<int> status{}; // 0 pending; 1 off command accepted; 2 failed; 3 wake failed
    std::thread worker;
    int reported=0;
};
std::vector<std::unique_ptr<PowerTask>> powerTasks;
std::map<std::string,std::wstring> powerStatus;
#ifdef DAC_HARNESS
std::atomic<int> fakePowerOff{},fakePowerOn{},fakePowerResult{},fakePowerDelay{},fakePowerWakeResult{};
int fakePowerProcess=0;
#endif
int ExecutePowerProcess(PowerTask* task,HANDLE cancelled,const std::wstring& executable,std::wstring command) {
    const auto& id=task->id;
    Handle job(CreateJobObjectW(nullptr,nullptr));JOBOBJECT_EXTENDED_LIMIT_INFORMATION limit{};limit.BasicLimitInformation.LimitFlags=JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if(!job||!SetInformationJobObject(job,JobObjectExtendedLimitInformation,&limit,sizeof(limit)))return 11;
    STARTUPINFOW si{};si.cb=sizeof(si);si.dwFlags=STARTF_USESHOWWINDOW;si.wShowWindow=SW_HIDE;PROCESS_INFORMATION pi{};
    if(!CreateProcessW(executable.c_str(),command.data(),nullptr,nullptr,FALSE,CREATE_SUSPENDED|CREATE_NO_WINDOW,nullptr,nullptr,&si,&pi))return 11;
    Handle process(pi.hProcess),thread(pi.hThread);
    if(!AssignProcessToJobObject(job,process)){TerminateProcess(process,1);WaitForSingleObject(process,5000);return 11;}
    if(ResumeThread(thread)==DWORD(-1)){TerminateJobObject(job,1);WaitForSingleObject(process,5000);return 11;}
    PowerDeadline deadline{GetTickCount64()};bool off=false;
    while(WaitForSingleObject(process,50)==WAIT_TIMEOUT) {
        auto now=GetTickCount64();std::string marker;DWORD error=0;
        if(!off&&ReadFileText(PowerMarker(id),marker,error)&&marker==id+"\n"+task->token+"\noff"){off=true;task->status=1;}
        int action=deadline.Sample(now,task->cancel||WaitForSingleObject(stopEvent,0)!=WAIT_TIMEOUT,off);
        if(action==1)SetEvent(cancelled);
        if(action==2){TerminateJobObject(job,1);WaitForSingleObject(process,5000);return 13;}
    }
    DWORD exit=1;GetExitCodeProcess(process,&exit);return static_cast<int>(exit);
}
int ExecutePower(PowerTask* task,HANDLE cancelled) {
    const auto& id=task->id;
#ifdef DAC_HARNESS
    if(fakePowerProcess){std::vector<wchar_t> path(32768);GetModuleFileNameW(nullptr,path.data(),32768);return ExecutePowerProcess(task,cancelled,path.data(),L"\""+std::wstring(path.data())+L"\" --power-helper "+Wide(Hex(id))+L" "+Wide(task->token)+L" "+std::to_wstring(fakePowerProcess));}
    return PowerCycle(id,task->token,[&]{return task->cancel||WaitForSingleObject(stopEvent,0)!=WAIT_TIMEOUT;},[&](bool wake){
        if(wake){++fakePowerOn;return int(fakePowerWakeResult);}
        ++fakePowerOff;if(fakePowerDelay)Sleep(fakePowerDelay);if(task->cancel||WaitForSingleObject(stopEvent,0)!=WAIT_TIMEOUT)return 10;
        DWORD code=0;int result=fakePowerResult;if(result!=10&&result!=11)WriteFileText(PowerMarker(id),id+"\n"+task->token+(result==0?"\noff":"\nchanging"),code);
        if(result==0)task->status=1;return result;
    });
#else
    std::vector<wchar_t> path(32768);DWORD size=GetModuleFileNameW(nullptr,path.data(),32768);if(!size||size>=32768)return 11;
    auto command=L"\""+std::wstring(path.data())+L"\" --power-helper "+Wide(Hex(id))+L" "+Wide(task->token)+L" "+std::to_wstring(GetCurrentProcessId());
    if(!configDirectoryOverride.empty())command+=L" --config-directory \""+configDirectoryOverride+L"\"";
    return ExecutePowerProcess(task,cancelled,path.data(),std::move(command));
#endif
}
void PowerWorker(PowerTask* task) {
    const auto path=PowerMarker(task->id);DWORD error=0;
    GUID guid{};if(FAILED(CoCreateGuid(&guid))){task->status=2;task->done=true;return;}
    task->token=Hex(std::string(reinterpret_cast<char*>(&guid),sizeof(guid)));
    Handle cancelled(CreateEventW(nullptr,TRUE,FALSE,PowerCancelName(task->token).c_str()));
    if(!cancelled||task->cancel||WaitForSingleObject(stopEvent,0)!=WAIT_TIMEOUT||PowerPending(task->id)||!WriteFileText(path,task->id+"\n"+task->token+"\nprobe",error)){task->status=2;task->done=true;return;}
    int result=ExecutePower(task,cancelled);std::string marker,prefix=task->id+"\n"+task->token+"\n";bool readable=ReadFileText(path,marker,error);
    if(result==0&&readable&&marker==prefix+"awake")task->status=DeleteFileW(Extended(path).c_str())?4:3;
    else if(readable&&marker==prefix+"untouched"){task->status=2;DeleteFileW(Extended(path).c_str());}
    else {task->status=3;if(!readable)WriteFileText(path,prefix+"fault",error);}
    // An interrupted/timed-out probe stays quarantined; no automatic retries.
    task->done=true;
}
void StopPower() {for(auto& task:powerTasks)task->cancel=true;}
int StartPower(const Display& display,Time generation) {
    if(FaultResetPending(display.id))return -1; // reserved until disk deletion completes
    if(!display.identified||!controller.Pref(display.id).hardware||controller.blocked)return 0;
    for(auto& task:powerTasks)if(task->id==display.id)return -1; // busy, not consumed
    auto task=std::make_unique<PowerTask>();task->id=display.id;task->generation=generation;
    try{task->worker=std::thread(PowerWorker,task.get());}catch(...){return false;}
    powerTasks.push_back(std::move(task));InvalidateFaults();return true;
}

// A run's worker exclusively owns its process and job. UI owns its HWND. The
// registry borrows the job only while published; emergency termination cannot
// race handle reuse. A stopped run is retained until worker completion.
struct Run {
    std::string id; Time generation{},began{}; HWND window{},preview{}; std::wstring path,args;
    std::atomic<bool> cancel{},abortChild{},done{}; std::atomic<int> status{}; // 0 launch, 1 heuristic child, 2 fallback
    std::thread worker; DWORD error=0; bool configuration=false,failureNotified=false,powerAttempted=false;
    Preference preference;bool contained=false;int presentation=-1;Time lastFrame=0;HFONT sceneFont=nullptr;~Run(){if(sceneFont)DeleteObject(sceneFont);}
    int width=0,height=0,padding=0;std::mutex frameLock;std::unique_ptr<Gdiplus::Bitmap> frame;
};
bool LocalFile(const std::wstring& path,const wchar_t* extension) {
    if(path.size()<4||path[1]!=L':'||path[2]!=L'\\'||path.size()>32760)return false;
    if(extension&&(path.size()<wcslen(extension)||_wcsicmp(path.c_str()+path.size()-wcslen(extension),extension)!=0))return false;
    return LocalPath(path,false);
}
void SlideFiles(const std::wstring& folder,bool recursive,std::vector<std::wstring>& files,Run* run,unsigned depth=0) {
    if(depth>16||files.size()>=2000||run->cancel||!LocalPath(folder,true))return;
    auto root=Extended(folder);DWORD attributes=GetFileAttributesW(root.c_str());if(attributes==INVALID_FILE_ATTRIBUTES||(attributes&FILE_ATTRIBUTE_REPARSE_POINT))return;
    WIN32_FIND_DATAW data{};HANDLE find=FindFirstFileW((root+L"\\*").c_str(),&data);if(find==INVALID_HANDLE_VALUE)return;
    do {
        if(run->cancel||WaitForSingleObject(stopEvent,0)!=WAIT_TIMEOUT||files.size()>=2000)break;
        if(data.cFileName[0]==L'.'&&(data.cFileName[1]==0||(data.cFileName[1]==L'.'&&data.cFileName[2]==0)))continue;
        if(data.dwFileAttributes&FILE_ATTRIBUTE_REPARSE_POINT)continue;
        auto path=folder+L"\\"+data.cFileName;
        if(data.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY){if(recursive)SlideFiles(path,recursive,files,run,depth+1);continue;}
        if(data.nFileSizeHigh||data.nFileSizeLow>32*1024*1024)continue;
        auto dot=path.find_last_of(L'.');if(dot==std::wstring::npos)continue;auto ext=path.substr(dot);
        if(_wcsicmp(ext.c_str(),L".jpg")==0||_wcsicmp(ext.c_str(),L".jpeg")==0||_wcsicmp(ext.c_str(),L".png")==0||_wcsicmp(ext.c_str(),L".bmp")==0||_wcsicmp(ext.c_str(),L".gif")==0)files.push_back(path);
    }while(FindNextFileW(find,&data));FindClose(find);
}
std::unique_ptr<Gdiplus::Bitmap> SlideFrame(const std::wstring& path,int width,int height,const Preference& p) {
    if(!LocalPath(path,false))return {};
    std::unique_ptr<Gdiplus::Image> source(Gdiplus::Image::FromFile(Extended(path).c_str(),FALSE));
    if(!source||source->GetLastStatus()!=Gdiplus::Ok||!source->GetWidth()||!source->GetHeight()||uint64_t(source->GetWidth())*source->GetHeight()>40000000)return {};
    double cap=std::min({1.0,4096.0/width,4096.0/height});width=std::max(1,int(width*cap));height=std::max(1,int(height*cap));
    auto frame=std::make_unique<Gdiplus::Bitmap>(width,height,PixelFormat32bppPARGB);if(frame->GetLastStatus()!=Gdiplus::Ok)return {};
    Gdiplus::Graphics graphics(frame.get());graphics.Clear(Gdiplus::Color(255,(p.background>>16)&255,(p.background>>8)&255,p.background&255));
    graphics.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
    double sw=source->GetWidth(),sh=source->GetHeight(),scale=std::min(width/sw,height/sh);
    if(p.placement==1)scale=std::max(width/sw,height/sh);if(p.placement==3||p.placement==4)scale=cap;if(p.placement==5)scale=std::min(cap,scale);
    float dw=p.placement==2?float(width):float(sw*scale),dh=p.placement==2?float(height):float(sh*scale);
    if(p.placement==4){Gdiplus::TextureBrush brush(source.get(),Gdiplus::WrapModeTile);brush.ScaleTransform(float(cap),float(cap));if(graphics.FillRectangle(&brush,0,0,width,height)!=Gdiplus::Ok)return {};}
    else if(graphics.DrawImage(source.get(),Gdiplus::RectF((width-dw)/2,(height-dh)/2,dw,dh))!=Gdiplus::Ok)return {};
    return frame;
}
void SlideWorker(Run* run) {
    try {
        std::vector<std::wstring> files;SlideFiles(Wide(run->preference.folder),run->preference.recursive,files,run);
        std::sort(files.begin(),files.end());std::mt19937 random(static_cast<unsigned>(GetTickCount64())^static_cast<unsigned>(reinterpret_cast<uintptr_t>(run)));
        if(run->preference.shuffle)std::shuffle(files.begin(),files.end(),random);
        size_t index=0,failures=0;
        while(!files.empty()&&!run->cancel&&!run->abortChild&&WaitForSingleObject(stopEvent,0)==WAIT_TIMEOUT) {
            auto frame=SlideFrame(files[index],run->width,run->height,run->preference);index=(index+1)%files.size();
            if(!frame){if(++failures>=files.size())break;continue;}failures=0;
            if(run->cancel||run->abortChild)break;
            {std::lock_guard lock(run->frameLock);run->frame=std::move(frame);}run->status=1;InvalidateRect(run->window,nullptr,FALSE);
            Time until=GetTickCount64()+static_cast<Time>(run->preference.slideSeconds)*1000;
            while(!run->cancel&&!run->abortChild&&GetTickCount64()<until&&WaitForSingleObject(stopEvent,50)==WAIT_TIMEOUT){}
            if(index==0&&run->preference.shuffle)std::shuffle(files.begin(),files.end(),random);
        }
    }catch(...){run->error=ERROR_NOT_ENOUGH_MEMORY;}
    run->status=2;run->done=true;
}
std::vector<std::unique_ptr<Run>> runs;
#ifdef DAC_HARNESS
// Only tests can substitute the owned fixture executable; product choices
// remain the six installed system paths. Main/UI thread exclusively owns this.
std::map<std::string,std::wstring> fixtureModes;
#endif
BOOL CALLBACK CloseOwned(HWND w,LPARAM lp) {
    HANDLE job=reinterpret_cast<HANDLE>(lp); DWORD pid=0; GetWindowThreadProcessId(w,&pid);
    Handle process(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,pid)); BOOL owned=FALSE;
    if(process && IsProcessInJob(process,job,&owned)&&owned) PostMessageW(w,WM_CLOSE,0,0); return TRUE;
}
struct ChildQuery { HANDLE job; HWND window=nullptr; };
BOOL CALLBACK FindOwnedChild(HWND w,LPARAM lp) {
    auto& q=*reinterpret_cast<ChildQuery*>(lp); DWORD pid=0; GetWindowThreadProcessId(w,&pid);
    Handle process(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,pid)); BOOL owned=FALSE;
    if(process&&IsProcessInJob(process,q.job,&owned)&&owned) {q.window=w;return FALSE;} return TRUE;
}
void RunWorker(Run* r) {
    Handle job(CreateJobObjectW(nullptr,nullptr)); JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
    limits.BasicLimitInformation.LimitFlags=JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    auto fail=[&](DWORD e) { r->error=e; r->status=2; Note(L"saver fallback",e); };
    if(!job||!SetInformationJobObject(job,JobObjectExtendedLimitInformation,&limits,sizeof(limits))) { fail(GetLastError()); r->done=true; return; }
    { std::lock_guard lock(registryLock); ownedJobs[job]=r->window; }
#ifdef DAC_HARNESS
    // A stalled launch simulation occurs after publication, with no registry lock.
    if(launchDelay) Sleep(launchDelay);
#endif
    PROCESS_INFORMATION pi{}; STARTUPINFOW si{}; si.cb=sizeof(si); si.dwFlags=STARTF_USESHOWWINDOW; si.wShowWindow=r->configuration?SW_SHOWNORMAL:SW_SHOWNOACTIVATE;
    std::wstring command=L"\""+r->path+L"\" "+r->args;
    if(r->cancel||r->abortChild||WaitForSingleObject(stopEvent,0)==WAIT_OBJECT_0) { }
    else if(!CreateProcessW(r->path.c_str(),command.data(),nullptr,nullptr,FALSE,CREATE_SUSPENDED,nullptr,nullptr,&si,&pi)) fail(GetLastError());
    else {
        Handle process(pi.hProcess),thread(pi.hThread);
        if(!AssignProcessToJobObject(job,process)) { fail(GetLastError()); TerminateProcess(process,1); WaitForSingleObject(process,2000); }
        else if(r->cancel||r->abortChild||WaitForSingleObject(stopEvent,0)==WAIT_OBJECT_0) TerminateJobObject(job,1);
        else if(ResumeThread(thread)==DWORD(-1)) { fail(GetLastError()); TerminateJobObject(job,1); }
        else {
            Note(L"owned process started",pi.dwProcessId);
            PreviewHealth health{r->began?r->began:GetTickCount64()};Time nextHealth=0;
            while(!r->cancel&&!r->abortChild&&WaitForSingleObject(stopEvent,50)==WAIT_TIMEOUT) {
                DWORD state=WaitForSingleObject(process,0);
                if(state!=WAIT_TIMEOUT) {
                    DWORD exitCode=0;
                    if(!r->configuration)fail(state==WAIT_OBJECT_0?ERROR_PROCESS_ABORTED:GetLastError());
                    else if(state!=WAIT_OBJECT_0||!GetExitCodeProcess(process,&exitCode)||exitCode!=0)fail(exitCode?exitCode:ERROR_PROCESS_ABORTED);
                    break;
                }
                auto now=GetTickCount64();
                if(!r->configuration&&now>=nextHealth) {
                    ChildQuery query{job}; EnumChildWindows(r->preview?r->preview:r->window,FindOwnedChild,reinterpret_cast<LPARAM>(&query));
                    DWORD_PTR ignored=0;
                    bool responsive=query.window&&SendMessageTimeoutW(query.window,WM_NULL,0,0,SMTO_BLOCK|SMTO_ABORTIFHUNG|SMTO_ERRORONEXIT,100,&ignored);
                    int result=health.Sample(now,query.window!=nullptr,responsive);
                    if(result==2) {fail(ERROR_TIMEOUT);break;}
                    if(result==1&&!r->abortChild) r->status=1;
                    nextHealth=now+(health.ready?1000:100);
                }
            }
            // Graceful close is posted, never sent synchronously to foreign UI.
            EnumWindows(CloseOwned,reinterpret_cast<LPARAM>(job.value));
            if(r->window) EnumChildWindows(r->window,CloseOwned,reinterpret_cast<LPARAM>(job.value));
            if(WaitForSingleObject(stopEvent,0)!=WAIT_OBJECT_0) WaitForSingleObject(process,750);
        }
        TerminateJobObject(job,0); // only this run's contained process tree
        if(WaitForSingleObject(process,2000)!=WAIT_OBJECT_0) Note(L"owned process exit wait expired");
    }
    { std::lock_guard lock(registryLock); ownedJobs.erase(job); }
    // job closes before done, so UI never destroys the parent while our child runs.
    if(job.value) { CloseHandle(job.value); job.value=nullptr; }
    r->done=true;
}

AppIdentity ProcessIdentity(DWORD pid) {
    AppIdentity result;Handle process(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,pid));std::vector<wchar_t> path(32768);DWORD size=32768;
    if(!process||!QueryFullProcessImageNameW(process,0,path.data(),&size))return result;
    std::wstring full(path.data(),size);CharLowerBuffW(full.data(),size);result.path=Utf8(full);auto slash=result.path.find_last_of("/\\");result.name=result.path.substr(slash==std::string::npos?0:slash+1);result.known=true;return result;
}
std::wstring ImageName(DWORD pid) {
    Handle process(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,pid));
    std::vector<wchar_t> path(32768); DWORD n=32768;
    if(!process||!QueryFullProcessImageNameW(process,0,path.data(),&n)) return {};
    std::wstring name(path.data(),n); auto slash=name.find_last_of(L"\\/"); if(slash!=std::wstring::npos) name.erase(0,slash+1);
    CharLowerBuffW(name.data(),static_cast<DWORD>(name.size())); return name;
}
bool IsOwned(DWORD pid) {
    Handle process(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,pid)); if(!process) return false;
    // Duplicate handles under the registry lock; OS ownership queries occur after release.
    std::vector<HANDLE> jobs;
    { std::lock_guard lock(registryLock); for(auto& [job,window]:ownedJobs) {
        (void)window; HANDLE copy=nullptr;
        if(DuplicateHandle(GetCurrentProcess(),job,GetCurrentProcess(),&copy,0,FALSE,DUPLICATE_SAME_ACCESS)) jobs.push_back(copy);
    } }
    bool owned=false; for(auto job:jobs) { BOOL inJob=FALSE; if(IsProcessInJob(process,job,&inJob)&&inJob) owned=true; CloseHandle(job); }
    return owned;
}
struct WindowMatch { DWORD pid; std::wstring name; const std::vector<Display>* displays; std::set<std::string> ids; };
BOOL CALLBACK MatchWindow(HWND w,LPARAM lp) {
    auto& q=*reinterpret_cast<WindowMatch*>(lp);
    if(!IsWindowVisible(w)||IsIconic(w)||(GetWindowLongPtrW(w,GWL_EXSTYLE)&WS_EX_TOOLWINDOW)) return TRUE;
    DWORD cloaked=0;if(SUCCEEDED(DwmGetWindowAttribute(w,DWMWA_CLOAKED,&cloaked,sizeof(cloaked)))&&cloaked) return TRUE;
    DWORD pid=0; GetWindowThreadProcessId(w,&pid);
    if(pid!=q.pid && (q.name.empty()||ImageName(pid)!=q.name)) return TRUE;
    RECT r{}; if(FAILED(DwmGetWindowAttribute(w,DWMWA_EXTENDED_FRAME_BOUNDS,&r,sizeof(r)))&&!GetWindowRect(w,&r)) return TRUE;
    bool matched=false;
    for(auto& d:*q.displays) if(MediaOverlap({r.left,r.top,r.right,r.bottom},{d.rect.left,d.rect.top,d.rect.right,d.rect.bottom})) {q.ids.insert(d.id);matched=true;}
    if(!matched) {POINT center{r.left+(r.right-r.left)/2,r.top+(r.bottom-r.top)/2};for(auto& d:*q.displays) if(PtInRect(&d.rect,center)) q.ids.insert(d.id);}
    return TRUE;
}
Media ObserveEndpoint(IMMDevice* endpoint,const Config& c,const std::vector<Display>& list) {
    Media m;m.at=GetTickCount64();HRESULT hr;
    Com<IAudioSessionManager2> manager;
    if(FAILED(endpoint->Activate(__uuidof(IAudioSessionManager2),CLSCTX_ALL,nullptr,reinterpret_cast<void**>(manager.out())))) return m;
    Com<IAudioEndpointVolume> endpointVolume; BOOL deviceMuted=FALSE; float deviceLevel=1;
    if(FAILED(endpoint->Activate(__uuidof(IAudioEndpointVolume),CLSCTX_ALL,nullptr,reinterpret_cast<void**>(endpointVolume.out())))||
       FAILED(endpointVolume->GetMute(&deviceMuted))||FAILED(endpointVolume->GetMasterVolumeLevelScalar(&deviceLevel))) return m;
    Com<IAudioSessionEnumerator> sessions;
    if(FAILED(manager->GetSessionEnumerator(sessions.out()))) return m;
    int count=0; if(FAILED(sessions->GetCount(&count))||count>1024) return m;
    m.valid=true;
    for(int i=0;i<count;++i) {
        Com<IAudioSessionControl> session; Com<IAudioSessionControl2> details; Com<MeterPeak> meter; Com<ISimpleAudioVolume> volume;
        AudioSessionState state; DWORD pid=0; float peak=0,level=1; BOOL mute=FALSE;
        if(FAILED(sessions->GetSession(i,session.out()))||FAILED(session->GetState(&state))) { m.valid=false; continue; }
        if(state!=AudioSessionStateActive) continue;
        if(FAILED(session->QueryInterface(__uuidof(IAudioSessionControl2),reinterpret_cast<void**>(details.out())))) { m.valid=false; continue; }
        hr=details->GetProcessId(&pid); if(FAILED(hr)) { m.valid=false; continue; }
        bool ambiguous=hr!=S_OK; // includes AUDCLNT_S_NO_SINGLE_PROCESS
        if(pid==GetCurrentProcessId()||IsOwned(pid)) continue;
        if(!c.muted) {
            if(FAILED(session->QueryInterface(meterIID,reinterpret_cast<void**>(meter.out())))||
               FAILED(session->QueryInterface(__uuidof(ISimpleAudioVolume),reinterpret_cast<void**>(volume.out())))||
               FAILED(meter->GetPeakValue(&peak))||FAILED(volume->GetMute(&mute))||FAILED(volume->GetMasterVolume(&level))) { m.valid=false; continue; }
        }
        if(!AudioBlocks(true,c.muted,mute,deviceMuted,peak,level,deviceLevel)) continue;
        auto identity=ProcessIdentity(pid);
        WindowMatch match{pid,ImageName(pid),&list,{}};
        EnumWindows(MatchWindow,reinterpret_cast<LPARAM>(&match));
        AddAudioContribution(m,c,identity,ambiguous,match.ids);
    }
    return m;
}
Media ObserveMedia(const Config& c,const std::vector<Display>& list) {
    Media m;m.at=GetTickCount64();bool owned=false;{std::lock_guard lock(registryLock);owned=!ownedJobs.empty();}
    if(c.muted&&!owned){ULONG state=0;if(CallNtPowerInformation(SystemExecutionState,nullptr,0,&state,sizeof(state))!=0)return m;
        if(DisplayRequestBlocks(c.muted,owned,(state&ES_DISPLAY_REQUIRED)!=0)){m.any=true;m.ambiguous=true;}}
    Com<IMMDeviceEnumerator> enumerator;Com<IMMDeviceCollection> endpoints;
    if(FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator),nullptr,CLSCTX_ALL,__uuidof(IMMDeviceEnumerator),reinterpret_cast<void**>(enumerator.out())))||FAILED(enumerator->EnumAudioEndpoints(eRender,DEVICE_STATE_ACTIVE,endpoints.out())))return m;
    UINT count=0;if(FAILED(endpoints->GetCount(&count))||count>64)return m;m.valid=true;
    for(UINT i=0;i<count;++i){Com<IMMDevice> endpoint;if(FAILED(endpoints->Item(i,endpoint.out()))){m.valid=false;continue;}
        auto sample=ObserveEndpoint(endpoint.p,c,list);m.valid=m.valid&&sample.valid;m.any=m.any||sample.any;m.ambiguous=m.ambiguous||sample.ambiguous;m.monitors.insert(sample.monitors.begin(),sample.monitors.end());}
    return m;
}
struct XInputAdapter {
    using GetState=DWORD(WINAPI*)(DWORD,XINPUT_STATE*);HMODULE module=nullptr;GetState get=nullptr;
    std::array<Time,4> next{};std::array<bool,4> connected{},held{};
    XInputAdapter(){for(auto dll:{L"xinput1_4.dll",L"xinput9_1_0.dll"}){module=LoadLibraryExW(dll,nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);if(module){get=reinterpret_cast<GetState>(GetProcAddress(module,"XInputGetState"));if(get)break;FreeLibrary(module);module=nullptr;}}}
    ~XInputAdapter(){if(module)FreeLibrary(module);}
    void Sample(Time now,bool enabled,AdapterSnapshot& snapshot){snapshot.controllerAvailable=get!=nullptr;snapshot.active=false;snapshot.connected=false;
        if(!enabled||!get){connected={};held={};next={};return;}
        for(unsigned i=0;i<4;++i){if(now>=next[i]){XINPUT_STATE state{};bool ok=get(i,&state)==ERROR_SUCCESS;bool before=held[i];connected[i]=ok;
                auto& g=state.Gamepad;held[i]=ok&&ControllerActive(g.wButtons&0xf3ff,g.sThumbLX,g.sThumbLY,g.sThumbRX,g.sThumbRY,g.bLeftTrigger,g.bRightTrigger);
                next[i]=now+(ok?250:2000);if(held[i]||(ok&&before&&!held[i])){snapshot.inputAt=now;snapshot.active=true;}}
            snapshot.connected=snapshot.connected||connected[i];}
    }
};
PowerSource ReadPowerSource(){SYSTEM_POWER_STATUS power{};if(!GetSystemPowerStatus(&power))return PowerSource::Unknown;return PowerFromStatus(true,power.ACLineStatus);}
DWORD WINAPI MediaMain(void*) {
    HRESULT com=CoInitializeEx(nullptr,COINIT_MULTITHREADED);XInputAdapter xinput;
    MediaGrace grace;uint64_t graceEpoch=0;Time nextMedia=0;AdapterSnapshot snapshot;
    while(WaitForSingleObject(stopEvent,0)==WAIT_TIMEOUT){Config c;std::vector<Display> list;uint64_t epoch;
        {std::lock_guard lock(observationLock);c=mediaConfig;list=mediaDisplays;epoch=observationEpoch;}
        auto now=GetTickCount64();snapshot.at=now;xinput.Sample(now,c.controllerInput,snapshot);snapshot.power=ReadPowerSource();
        {std::lock_guard lock(observationLock);adapters=snapshot;}
        if(epoch!=graceEpoch){grace={};graceEpoch=epoch;nextMedia=0;}
        if(now>=nextMedia){bool needed=c.media;for(auto& [id,p]:c.monitors){(void)id;if(p.enabled&&p.media==1)needed=true;}
            Media m;if(SUCCEEDED(com)&&needed)m=ObserveMedia(c,list);else if(!needed){m.valid=true;m.at=now;}m=grace.Apply(m,c.poll);
            {std::lock_guard lock(observationLock);if(epoch==observationEpoch)observation=std::move(m);}nextMedia=now+c.poll;}
        if(WaitForSingleObject(stopEvent,250)!=WAIT_TIMEOUT)break;
    }if(SUCCEEDED(com))CoUninitialize();return 0;
}
void InvalidateMedia() {
    std::lock_guard lock(observationLock); ++observationEpoch; observation={}; mediaConfig=controller.config;static_cast<Policy&>(mediaConfig)=controller.PolicyNow();mediaDisplays=displays;
}
void StopRuns() {
    StopPower();
    for(auto& r:runs) { r->cancel=true; if(r->window) ShowWindowAsync(r->window,SW_HIDE); }
}
bool AddRun(const Display& d,Time generation,int saver,bool configuration=false) {
    if(configuration&&saver>=7)return false;
    if(controller.blocked||WaitForSingleObject(stopEvent,0)!=WAIT_TIMEOUT) return false;
    if(runs.size()>=64) { Note(L"too many stopping sessions"); return false; }
    for(auto& r:runs) if(r->id==d.id&&!r->cancel) return false;
    auto r=std::make_unique<Run>(); r->id=d.id; r->generation=generation; r->configuration=configuration; r->began=GetTickCount64();
    r->presentation=saver;r->padding=saver==-3?0:controller.PolicyNow().padding;r->preference=controller.Pref(d.id);r->width=d.rect.right-d.rect.left;r->height=d.rect.bottom-d.rect.top;
    if(d.id=="@span")r->preference=controller.Pref(spanningDisplay.alias);
    if(!configuration) {
        auto b=Padded({d.rect.left,d.rect.top,d.rect.right,d.rect.bottom},r->padding);
        r->window=CreateWindowExW(WS_EX_TOPMOST|WS_EX_NOACTIVATE|WS_EX_TOOLWINDOW|(saver==-3?(WS_EX_LAYERED|WS_EX_TRANSPARENT):0),kClass,L"DAC — Display Activity Controls",WS_POPUP|WS_CLIPCHILDREN,
            static_cast<int>(b.left),static_cast<int>(b.top),static_cast<int>(b.right-b.left),static_cast<int>(b.bottom-b.top),nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
        if(!r->window) return false;
        { std::lock_guard lock(registryLock); if(WaitForSingleObject(stopEvent,0)!=WAIT_TIMEOUT) {DestroyWindow(r->window);return false;} ownedWindows.insert(r->window); }
        if(saver==-3&&!SetLayeredWindowAttributes(r->window,0,static_cast<BYTE>(DimAlpha(r->preference.dim,r->preference.fadeMs,0)),LWA_ALPHA)){DestroyWindow(r->window);{std::lock_guard lock(registryLock);ownedWindows.erase(r->window);}Note(L"dim alpha unavailable",GetLastError());return false;}
        if(saver>=0&&saver<=6) {
            // Padding is black; the saver gets the exact monitor-sized child.
            r->preview=CreateWindowExW(WS_EX_NOACTIVATE,kClass,L"",WS_CHILD|WS_VISIBLE|WS_CLIPCHILDREN,
                r->padding,r->padding,d.rect.right-d.rect.left,d.rect.bottom-d.rect.top,r->window,nullptr,GetModuleHandleW(nullptr),nullptr);
            if(!r->preview) { {std::lock_guard lock(registryLock);ownedWindows.erase(r->window);} DestroyWindow(r->window); return false; }
        }
#ifndef DAC_HARNESS
        { std::lock_guard lock(registryLock); if(WaitForSingleObject(stopEvent,0)==WAIT_TIMEOUT) ShowWindowAsync(r->window,SW_SHOWNOACTIVATE); }
#endif
    }
    if(saver>=0&&saver<=7) {
        r->path=saver==6?Wide(r->preference.custom):SaverPath(saver);if(saver==6&&!LocalFile(r->path,L".scr"))r->path.clear();
        r->args=configuration?L"/c:"+std::to_wstring(reinterpret_cast<uintptr_t>(ui.load())):L"/p "+std::to_wstring(reinterpret_cast<uintptr_t>(r->preview));
#ifdef DAC_HARNESS
        if(auto it=fixtureModes.find(d.id);it!=fixtureModes.end()) {
            wchar_t exe[32768];DWORD size=GetModuleFileNameW(nullptr,exe,32768);
            if(size&&size<32768) {r->path=exe;r->args=it->second+L" "+std::to_wstring(reinterpret_cast<uintptr_t>(r->preview));}
        }
#endif
        try { r->worker=std::thread(saver==7?SlideWorker:RunWorker,r.get()); }
        catch(...) { if(r->window) { {std::lock_guard lock(registryLock);ownedWindows.erase(r->window);} DestroyWindow(r->window); } Note(L"worker creation failed"); return false; }
    } else {r->done=true;if(saver>=8)r->status=1;}
    runs.push_back(std::move(r)); return true;
}
void PaintScene(HDC dc,const RECT& bounds,Run& r,Time now) {
    int width=bounds.right-bounds.left,height=bounds.bottom-bounds.top;if(width<=0||height<=0)return;
    Time elapsed=now>=r.began?now-r.began:0;int scene=r.presentation;if(r.preference.rotateSeconds>0)scene=8+((elapsed/(static_cast<Time>(r.preference.rotateSeconds)*1000))%2);
    COLORREF colors[]={RGB(64,64,64),RGB(50,35,68),RGB(25,52,55)};SetTextColor(dc,colors[std::clamp(r.preference.sceneTheme,0,2)]);SetBkMode(dc,TRANSPARENT);
    if(scene==8){if(!r.sceneFont)r.sceneFont=CreateFontW(-std::clamp(height/9,16,96),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,DEFAULT_PITCH,L"Segoe UI");
        auto old=r.sceneFont?SelectObject(dc,r.sceneFont):nullptr;SYSTEMTIME time{};GetLocalTime(&time);wchar_t clock[32];swprintf_s(clock,L"%02u:%02u",time.wHour,time.wMinute);SIZE size{};GetTextExtentPoint32W(dc,clock,5,&size);
        double seconds=elapsed/1000.0;int x=std::max(0,int((width-size.cx)*(0.5+0.46*std::sin(seconds/19.0))));int y=std::max(0,int((height-size.cy)*(0.5+0.46*std::sin(seconds/23.0+1))));TextOutW(dc,x,y,clock,5);if(old)SelectObject(dc,old);
    }else{auto old=SelectObject(dc,GetStockObject(DC_PEN));SetDCPenColor(dc,colors[std::clamp(r.preference.sceneTheme,0,2)]);double seconds=elapsed/1000.0;
        for(int i=0;i<6;++i){int x=int(width*(0.5+0.46*std::sin(seconds/(13+i)+i*1.7)));int y=int(height*(0.5+0.46*std::cos(seconds/(17+i)+i*2.1)));MoveToEx(dc,x-2,y,nullptr);LineTo(dc,x+3,y);MoveToEx(dc,x,y-2,nullptr);LineTo(dc,x,y+3);}SelectObject(dc,old);}
}
bool StartDraftPreview(const Preference& preference) {
    for(auto& r:runs)if(r->contained&&!r->cancel){r->cancel=true;ShowWindowAsync(r->window,SW_HIDE);}
    if(runs.size()>=64||WaitForSingleObject(stopEvent,0)!=WAIT_TIMEOUT)return false;
    auto r=std::make_unique<Run>();r->id="@preview";r->contained=true;r->preference=preference;r->preference.hardware=false;r->preference.powerAfter=0;r->presentation=preference.saver;r->began=GetTickCount64();r->width=640;r->height=400;
    r->window=CreateWindowExW(WS_EX_APPWINDOW,kClass,L"Draft preview — unsaved; closes after 2 minutes",WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN,CW_USEDEFAULT,CW_USEDEFAULT,660,450,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);if(!r->window)return false;
    {std::lock_guard lock(registryLock);ownedWindows.insert(r->window);}
    if(r->presentation>=0&&r->presentation<=6){r->preview=CreateWindowExW(0,kClass,L"",WS_CHILD|WS_VISIBLE|WS_CLIPCHILDREN,0,0,640,400,r->window,nullptr,GetModuleHandleW(nullptr),nullptr);if(!r->preview)r->presentation=-1;}
    if(r->presentation>=0&&r->presentation<=7){r->path=r->presentation==6?Wide(preference.custom):SaverPath(r->presentation);if(r->presentation==6&&!LocalFile(r->path,L".scr"))r->path.clear();r->args=L"/p "+std::to_wstring(reinterpret_cast<uintptr_t>(r->preview));
#ifdef DAC_HARNESS
        if(auto it=fixtureModes.find("@preview");it!=fixtureModes.end()){wchar_t exe[32768];GetModuleFileNameW(nullptr,exe,32768);r->path=exe;r->args=it->second+L" "+std::to_wstring(reinterpret_cast<uintptr_t>(r->preview));}
#endif
        try{r->worker=std::thread(r->presentation==7?SlideWorker:RunWorker,r.get());}catch(...){std::lock_guard lock(registryLock);ownedWindows.erase(r->window);DestroyWindow(r->window);return false;}
    }else r->done=true;
#ifndef DAC_HARNESS
    ShowWindow(r->window,SW_SHOWNORMAL);
#endif
    runs.push_back(std::move(r));return true;
}
// Input dismissal only cancels stale owners and hides their native shells. It
// never launches work, queries files, joins workers or reports shell notices.
void ReconcileWake() {
    Measure measure{wakeTiming.reconcileMaxUs};Count(wakeTiming.reconcileEntries);
    for(auto& task:powerTasks){auto n=controller.nodes.find(task->id);if(n==controller.nodes.end()||n->second.generation!=task->generation||!Running(n->second.state))if(!task->cancel.exchange(true))Count(wakeTiming.newlyCancelledPower);}
    for(auto& r:runs)if(!r->contained&&!r->configuration){auto n=controller.nodes.find(r->id);if(n==controller.nodes.end()||n->second.generation!=r->generation||!Running(n->second.state)){if(!r->cancel.exchange(true))Count(wakeTiming.newlyCancelled);
        if(r->window){
            Count(wakeTiming.hideRequests);
            if(!IsWindow(r->window))Count(wakeTiming.preInvalid);else Count(IsWindowVisible(r->window)?wakeTiming.preVisible:wakeTiming.preHidden);
            ShowWindow(r->window,SW_HIDE); // Return value describes previous visibility, not hiding success.
            if(!IsWindow(r->window))Count(wakeTiming.postInvalid);else Count(IsWindowVisible(r->window)?wakeTiming.postVisible:wakeTiming.postHidden);
        }}}
}
void Reconcile() {
    for(auto it=powerTasks.begin();it!=powerTasks.end();) {
        auto& task=**it;auto n=controller.nodes.find(task.id);
        if(n==controller.nodes.end()||n->second.generation!=task.generation||!Running(n->second.state)||controller.blocked||!controller.Pref(task.id).enabled||!controller.Pref(task.id).hardware)task.cancel=true;
        bool completed=task.done.load();if(completed&&task.worker.joinable())task.worker.join();
        int status=task.status;
        if(status!=task.reported) {
            task.reported=status;
            powerStatus[task.id]=status==1?L"Hardware off command accepted":status==2?L"Hardware unavailable; black fallback":status==3?L"Hardware wake failed; use monitor button":L"Hardware wake command accepted";
            std::string alias="host";for(size_t i=0;i<displays.size();++i)if(displays[i].id==task.id){alias="Display "+std::to_string(i+1);break;}
            Record(alias,status==1?"hardware off accepted":status==2?"hardware unavailable":status==3?"hardware wake failed":"hardware wake accepted");
            if(status==2)Notice(L"Hardware power request failed or is quarantined. Black fallback remains. Check the monitor's DDC/CI setting; no automatic retry in this idle session.");
            if(status==3)Notice(L"Monitor wake failed. Use its physical power button. Hardware control remains quarantined until Reset hardware fault in Settings.");
        }
        if(completed){it=powerTasks.erase(it);InvalidateFaults();}else ++it;
    }
    for(auto it=runs.begin();it!=runs.end();) {
        auto& r=**it; auto n=controller.nodes.find(r.id);
        if(r.contained&&GetTickCount64()-r.began>=120000)r.cancel=true;
        // Retire expensive content behind the existing opaque native shell.
        if(!r.contained&&!r.configuration&&!r.cancel&&n!=controller.nodes.end()&&n->second.state==State::Black&&n->second.generation!=r.generation){r.abortChild=true;r.generation=n->second.generation;r.presentation=-1;{std::lock_guard lock(r.frameLock);r.frame.reset();}if(r.preview)ShowWindow(r.preview,SW_HIDE);if(GetWindowLongPtrW(r.window,GWL_EXSTYLE)&WS_EX_LAYERED)SetLayeredWindowAttributes(r.window,0,255,LWA_ALPHA);InvalidateRect(r.window,nullptr,FALSE);}
        if(!r.contained&&!r.configuration&&(n==controller.nodes.end()||n->second.generation!=r.generation||!Running(n->second.state))) r.cancel=true;
        if(r.cancel&&r.window) ShowWindowAsync(r.window,SW_HIDE);
        if(r.done&&(r.cancel||r.configuration)) {
            if(r.configuration&&!r.cancel&&r.status==2) {
                Notice(L"Saver configuration could not start. Check that the selected saver is installed and supports configuration.");
                Note(L"configuration failed",r.error);
#ifdef DAC_HARNESS
                ++configurationFailures;
#endif
            }
            if(r.worker.joinable()) r.worker.join(); if(r.window) { {std::lock_guard lock(registryLock);ownedWindows.erase(r.window);} DestroyWindow(r.window); }
            it=runs.erase(it); continue;
        }
        if(!r.cancel&&!r.configuration) {
            auto now=GetTickCount64();
            if(r.presentation==-3)SetLayeredWindowAttributes(r.window,0,static_cast<BYTE>(DimAlpha(r.preference.dim,r.preference.fadeMs,now-r.began)),LWA_ALPHA);
            if(r.presentation>=8&&now-r.lastFrame>=100){r.lastFrame=now;InvalidateRect(r.window,nullptr,FALSE);}
            if(r.contained){if(r.status.exchange(0)==2||((r.presentation>=8)&&now-r.began>=static_cast<Time>(r.preference.sceneSeconds)*1000)){if(r.presentation!=-1){r.presentation=-1;{std::lock_guard lock(r.frameLock);r.frame.reset();}InvalidateRect(r.window,nullptr,FALSE);}}++it;continue;}
            int status=r.status.exchange(0);
            if(status!=1&&n!=controller.nodes.end()&&n->second.state==State::Launching&&GetTickCount64()-r.began>=5000) {
                r.abortChild=true; status=2;
            }
            if(r.abortChild&&n!=controller.nodes.end()&&n->second.state!=State::Black) status=2;
            if(status==2&&r.presentation!=-1){r.presentation=-1;std::lock_guard lock(r.frameLock);r.frame.reset();if(r.window)InvalidateRect(r.window,nullptr,FALSE);}
            if(status&&controller.Result(r.id,r.generation,status==1)&&status==2&&!r.failureNotified) {
                Notice(L"Saver exited, failed to start or stopped responding. Black fallback remains until activity or Stop; no automatic retry in this session.");
                r.failureNotified=true;
            }
            if(!r.powerAttempted&&r.done&&n!=controller.nodes.end()&&(n->second.powerRequested||PowerDue(controller.Pref(r.id),n->second.state,n->second.began,GetTickCount64()))) {
                auto display=std::find_if(displays.begin(),displays.end(),[&](const Display& d){return d.id==r.id;});
                if(display!=displays.end())r.powerAttempted=StartPower(*display,r.generation)>=0;
            }
        } ++it;
    }
    for(auto& d:displays) {
        auto& n=controller.nodes[d.id]; if(!Running(n.state)) continue;
        bool exists=false; for(auto& r:runs) if(r->id==d.id) { exists=true; break; }
        if(!exists&&!AddRun(d,n.generation,n.state==State::Dim?-3:n.presentation==-2?controller.Pref(d.id).saver:n.presentation)) { n.state=State::Desktop; n.last=GetTickCount64(); Notice(L"Unable to create presentation; activation delayed."); }
    }
    auto span=controller.nodes.find("@span");
    if(span!=controller.nodes.end()) {
        if(!Running(span->second.state))controller.nodes.erase(span);
        else {bool exists=false;for(auto& r:runs)if(r->id=="@span")exists=true;
            if(!exists&&!AddRun(spanningDisplay,span->second.generation,span->second.presentation))controller.nodes.erase(span);}
    }
}
void Reset(ResetCause cause=ResetCause::Other) {Count(wakeTiming.resets[static_cast<size_t>(cause)]); controller.Reset(GetTickCount64()); StopRuns(); nextPoll=0; }
void RefreshTopology() {
    Reset(ResetCause::Topology); displays=Catalog(); std::vector<std::string> ids;
    for(auto& d:displays) ids.push_back(d.id);
    controller.Topology(ids,GetTickCount64());controller.unidentified.clear();for(auto& d:displays)if(!d.identified)controller.unidentified.insert(d.id);InvalidateMedia();InvalidateFaults();topologyPending=false;
}
void SetBlocked(ResetCause cause=ResetCause::Other) {controller.blocked=locked||suspended||displayOff||!trayPresent;controller.candidateSince=GetTickCount64();controller.candidateProfile.clear();Reset(cause);InvalidateMedia();}
std::set<std::string> ForegroundDisplays(HWND window) {
    std::set<std::string> ids;RECT bounds{};if(!window||!IsWindowVisible(window)||IsIconic(window)||!GetWindowRect(window,&bounds))return ids;
    for(auto& d:displays)if(MediaOverlap({bounds.left,bounds.top,bounds.right,bounds.bottom},{d.rect.left,d.rect.top,d.rect.right,d.rect.bottom}))ids.insert(d.id);return ids;
}
// Expand the low 32-bit queue timestamp against the same uptime sample. Ages
// at or beyond half the DWORD range are ambiguous (including future stamps).
bool MessageTimeAt(Time now,DWORD stamp,Time& at,DWORD& age){age=static_cast<DWORD>(now)-stamp;if(age>=0x80000000u||age>now)return false;at=now-age;return true;}
MSG dispatchedMessage{};
void DeliverInput(const RAWINPUT& input,const MSG& message,Time now) {
    Measure measure{wakeTiming.inputMaxUs,&wakeTiming.dispatchUs};Count(wakeTiming.inputs);ObserveCoverage();
    bool keyboard=input.header.dwType==RIM_TYPEKEYBOARD;if(!keyboard&&input.header.dwType!=RIM_TYPEMOUSE){Count(wakeTiming.unsupported);return;}
    Time at=0;DWORD age=0;if(!MessageTimeAt(now,message.time,at,age)){Count(wakeTiming.invalidTime);return;}
    Count(wakeTiming.validTime);Maximum(wakeTiming.queueMaxMs,age);Sample(wakeTiming.queueMs,age);
    if(at<=controller.inputBoundary||topologyPending){Count(wakeTiming.boundaryRejected);if(topologyPending)Count(wakeTiming.topologyRejected);if(at<=controller.inputBoundary)Count(wakeTiming.timestampBoundary);return;}
    bool ambiguous=false;std::string cursor;for(auto& d:displays)if(PtInRect(&d.rect,message.pt)){if(!cursor.empty()){ambiguous=true;cursor.clear();break;}cursor=d.id;}
    Count(ambiguous?wakeTiming.pointerAmbiguous:cursor.empty()?wakeTiming.pointerAbsent:wakeTiming.pointerUnique);
    bool fresh=age<=250;auto focus=fresh?ForegroundDisplays(GetForegroundWindow()):std::set<std::string>{};
    if(!fresh)Count(wakeTiming.focusUnavailable);
    Count(fresh&&!focus.empty()?wakeTiming.foregroundFresh:wakeTiming.foregroundUnavailable);
    unsigned accepted=controller.Activity(now,keyboard,cursor,focus,true,at,fresh);
    Count(wakeTiming.accepted,accepted);
    if(!accepted){bool sticky=false,boundary=false,inhibited=false;
        for(auto& [id,n]:controller.nodes){auto p=controller.Pref(id);int mode=p.input<0?(controller.PolicyNow().perInput?1:0):p.input;
            bool pointer=cursor==id,foreground=fresh&&focus.contains(id);
            bool hit=id=="@span"||mode==0||(mode==2?pointer:mode==3?foreground:pointer||(keyboard&&foreground));if(!hit)continue;
            sticky=sticky||n.sticky;boundary=boundary||at<n.last||(n.manual&&at<=n.last)||(Running(n.state)&&at<n.began);inhibited=inhibited||!p.enabled||controller.paused||controller.blocked;
        }
        if(boundary)Count(wakeTiming.boundaryRejected);else if(sticky)Count(wakeTiming.stickyRejected);else if(inhibited)Count(wakeTiming.inhibited);else Count(wakeTiming.unattributed);
    }
    ReconcileWake();
}
void Input(HRAWINPUT handle) {
    Measure measure{wakeTiming.inputMaxUs,&wakeTiming.rawUs};
#ifdef DAC_HARNESS
    if(injectedObservations)return;
#endif
    // MSG keeps full signed coordinates. Nested modal loops dispatch outside
    // UiMain: use their thread's signed GetMessagePos/time fallback immediately.
    Count(wakeTiming.rawCalls);MSG message=dispatchedMessage;DWORD stamp=static_cast<DWORD>(GetMessageTime());
    if(message.message!=WM_INPUT||message.lParam!=reinterpret_cast<LPARAM>(handle)||message.time!=stamp){Count(wakeTiming.nestedMetadata);message={};message.time=stamp;DWORD position=GetMessagePos();message.pt={static_cast<short>(LOWORD(position)),static_cast<short>(HIWORD(position))};}else Count(wakeTiming.directMetadata);
    RAWINPUT input{};UINT bytes=sizeof(input);auto now=GetTickCount64();
    if(GetRawInputData(handle,RID_INPUT,&input,&bytes,sizeof(RAWINPUTHEADER))==UINT(-1)){
        Count(wakeTiming.readFailures);Count(wakeTiming.rawFailure);Time at=0;DWORD age=0;
        if(topologyPending)Count(wakeTiming.topologyRejected);
        else if(MessageTimeAt(now,message.time,at,age)){
            Count(wakeTiming.validTime);Maximum(wakeTiming.queueMaxMs,age);Sample(wakeTiming.queueMs,age);
            if(at<=controller.inputBoundary){Count(wakeTiming.timestampBoundary);Count(wakeTiming.boundaryRejected);}
            controller.Input(now,{},false,at);ReconcileWake();
        }else Count(wakeTiming.invalidTime);
        return;
    }
    Count(wakeTiming.readSuccess);
    rawStamp=message.time;DeliverInput(input,message,now);
}
void CheckInputFallback(){LASTINPUTINFO last{sizeof(last),0};if(!GetLastInputInfo(&last)){Count(wakeTiming.readFailures);Count(wakeTiming.fallbackReadFailure);return;}Time now=GetTickCount64(),at=0;DWORD age=0;if(last.dwTime!=lastInputStamp&&last.dwTime!=rawStamp&&MessageTimeAt(now,last.dwTime,at,age))controller.Input(now,{},false,at);lastInputStamp=last.dwTime;}
void ObserveForeground() {
    HWND window=GetForegroundWindow();DWORD pid=0;GetWindowThreadProcessId(window,&pid);controller.fullscreen.clear();
    auto ids=ForegroundDisplays(window);bool owned=pid==GetCurrentProcessId()||IsOwned(pid);controller.foreground={ProcessIdentity(pid),ids,window!=nullptr,owned};if(owned){controller.foreground.app={};controller.foreground.monitors.clear();}
    RECT bounds{};if(window&&!owned&&IsWindowVisible(window)&&!IsIconic(window)&&GetWindowRect(window,&bounds))for(auto& d:displays)if(bounds.left<=d.rect.left&&bounds.top<=d.rect.top&&bounds.right>=d.rect.right&&bounds.bottom>=d.rect.bottom)controller.fullscreen.insert(d.id);
    // Focus changes (including those caused by dismissing a presentation) are
    // observations for media/profile rules, not independent input events.
}
// One joined owner performs filesystem/shell maintenance. At most one request,
// one result and 32 reset reservations exist. No external call holds this lock.
struct MaintenanceRequest {
    Time faultEpoch=0,trayEpoch=0;std::vector<std::pair<std::string,std::wstring>> paths,resets;
    bool tray=false,remove=false,present=false;int trayState=0;HWND window{};HICON icon{};std::wstring notice;
};
struct MaintenanceResult {
    Time faultEpoch=0,trayEpoch=0;std::map<std::string,int> faults;std::set<std::string> resetFailed,resetDone;
    bool tray=false,present=false;
};
std::mutex maintenanceLock;std::condition_variable maintenanceChanged;std::thread maintenanceWorker;
MaintenanceRequest maintenancePending;MaintenanceResult maintenanceResult;bool maintenanceResultReady=false,maintenanceStop=false;
Time faultEpoch=1,trayEpoch=1,nextFault=0;bool faultsDirty=true,trayOutstanding=false;int requestedTray=-1;
std::map<std::string,int> faultCache;std::set<std::string> resettingFaults;
#ifdef DAC_HARNESS
std::atomic<DWORD> maintenanceFileDelay{},maintenanceTrayDelay{};
std::atomic<bool> maintenanceFileFail{},maintenanceInFile{},maintenanceInTray{};
#endif
void InvalidateFaults(){++faultEpoch;faultsDirty=true;faultCache.clear();}
bool CachedFault(const std::string& id){auto i=faultCache.find(id);return i==faultCache.end()||i->second;}
const char* FaultCacheState(const std::string& id){auto i=faultCache.find(id);return i==faultCache.end()?"refresh pending":i->second==2?"query failed":i->second==1?"marker retained":"clear";}
bool FaultResetPending(const std::string& id){return resettingFaults.contains(id);}
void QueueNotice(const wchar_t* text){std::lock_guard lock(maintenanceLock);maintenancePending.notice=std::wstring(text).substr(0,255);maintenanceChanged.notify_one();}
void RequestFaultReset(const std::string& id){
    for(auto& task:powerTasks)if(task->id==id)return;
    if(resettingFaults.size()>=32||resettingFaults.contains(id))return;
    resettingFaults.insert(id);InvalidateFaults();
    std::lock_guard lock(maintenanceLock);maintenancePending.resets.emplace_back(id,PowerMarker(id));maintenanceChanged.notify_one();
}
void MaintenanceMain(){
    HWND window=ui;bool shellPresent=false;
    while(true){MaintenanceRequest request;
        {std::unique_lock lock(maintenanceLock);maintenanceChanged.wait(lock,[]{return maintenanceStop||(!maintenanceResultReady&&(maintenancePending.faultEpoch||maintenancePending.tray||!maintenancePending.resets.empty()||!maintenancePending.notice.empty()));});
            if(maintenanceStop)break;request=std::move(maintenancePending);maintenancePending={};}
        MaintenanceResult result;result.faultEpoch=request.faultEpoch;result.trayEpoch=request.trayEpoch;
        if(request.faultEpoch||!request.resets.empty()){
            Measure measure{wakeTiming.fileMaxUs};
#ifdef DAC_HARNESS
            maintenanceInFile=true;if(auto delay=maintenanceFileDelay.load())Sleep(delay);
#endif
            for(auto& [id,path]:request.resets){
                bool removed=!!DeleteFileW(Extended(path).c_str());DWORD error=removed?ERROR_SUCCESS:GetLastError();
                if(!removed&&error!=ERROR_FILE_NOT_FOUND&&error!=ERROR_PATH_NOT_FOUND)result.resetFailed.insert(id);
                result.resetDone.insert(id);
            }
            for(auto& [id,path]:request.paths){DWORD attributes=GetFileAttributesW(Extended(path).c_str());DWORD error=attributes==INVALID_FILE_ATTRIBUTES?GetLastError():ERROR_SUCCESS;
#ifdef DAC_HARNESS
                if(maintenanceFileFail){attributes=INVALID_FILE_ATTRIBUTES;error=ERROR_ACCESS_DENIED;}
#endif
                bool failed=attributes==INVALID_FILE_ATTRIBUTES&&error!=ERROR_FILE_NOT_FOUND&&error!=ERROR_PATH_NOT_FOUND;
                if(failed)Count(wakeTiming.fileFailures);result.faults[id]=failed?2:attributes!=INVALID_FILE_ATTRIBUTES?1:0;
            }
#ifdef DAC_HARNESS
            maintenanceInFile=false;
#endif
        }
        bool stopping=false;{std::lock_guard lock(maintenanceLock);stopping=maintenanceStop;}
        if(!stopping&&(request.tray||!request.notice.empty())){
            Measure measure{wakeTiming.trayMaxUs};
#ifdef DAC_HARNESS
            maintenanceInTray=true;if(auto delay=maintenanceTrayDelay.load())Sleep(delay);
#endif
            if(request.tray){Count(wakeTiming.trayCalls);result.tray=true;
#ifdef DAC_HARNESS
                int failures=trayFailures.load();if(failures>0)trayFailures.fetch_sub(1);shellPresent=!request.remove&&failures<=0;
#else
                NOTIFYICONDATAW n{};n.cbSize=sizeof(n);n.hWnd=request.window;n.uID=1;n.uFlags=NIF_MESSAGE|NIF_ICON|NIF_TIP;n.uCallbackMessage=kTray;n.hIcon=request.icon;
                const wchar_t* states[]={L"manual only",L"watching idle",L"presenting",L"paused"};auto tip=std::wstring(L"DAC — Display Activity Controls — ")+states[request.trayState];wcsncpy_s(n.szTip,tip.c_str(),_TRUNCATE);
                bool ok=!!Shell_NotifyIconW(request.remove?NIM_DELETE:shellPresent?NIM_MODIFY:NIM_ADD,&n);
                shellPresent=!request.remove&&ok;
#endif
                result.present=shellPresent;
            }
#ifndef DAC_HARNESS
            if(!request.notice.empty()){NOTIFYICONDATAW n{};n.cbSize=sizeof(n);n.hWnd=window;n.uID=1;n.uFlags=NIF_INFO;wcscpy_s(n.szInfoTitle,L"DAC — Display Activity Controls");wcsncpy_s(n.szInfo,request.notice.c_str(),_TRUNCATE);Shell_NotifyIconW(NIM_MODIFY,&n);}
#endif
#ifdef DAC_HARNESS
            maintenanceInTray=false;
#endif
        }
        if(request.icon)DestroyIcon(request.icon);
        {std::lock_guard lock(maintenanceLock);if(!maintenanceStop){maintenanceResult=std::move(result);maintenanceResultReady=true;}}
    }
#ifndef DAC_HARNESS
    if(window){NOTIFYICONDATAW n{};n.cbSize=sizeof(n);n.hWnd=window;n.uID=1;Shell_NotifyIconW(NIM_DELETE,&n);}
#else
    (void)window;(void)shellPresent;
#endif
}
bool Tray(bool remove=false){
    MaintenanceRequest request;request.tray=true;request.remove=remove;request.present=trayPresent;request.window=ui;
    request.trayState=controller.paused?3:controller.Any()?2:controller.config.automatic?1:0;
    request.icon=remove?nullptr:CopyIcon(controller.Any()?theme.activeIcon:theme.smallIcon);request.trayEpoch=++trayEpoch;
    trayOutstanding=true;requestedTray=request.trayState;nextTray=GetTickCount64()+(trayPresent?30000:1000);
    std::lock_guard lock(maintenanceLock);if(maintenancePending.icon)DestroyIcon(maintenancePending.icon);
    maintenancePending.tray=true;maintenancePending.remove=remove;maintenancePending.present=request.present;maintenancePending.window=request.window;
    maintenancePending.trayState=request.trayState;maintenancePending.icon=request.icon;maintenancePending.trayEpoch=request.trayEpoch;maintenanceChanged.notify_one();return true;
}
void Maintain(Time now){
    MaintenanceResult result;bool got=false;
    {std::lock_guard lock(maintenanceLock);if(maintenanceResultReady){result=std::move(maintenanceResult);maintenanceResult={};maintenanceResultReady=false;got=true;maintenanceChanged.notify_one();}}
    if(got){
        // A reset reservation survives epoch invalidation until its actual disk
        // operation completes. StartPower cannot create a ticket during deletion.
        for(auto& id:result.resetDone){resettingFaults.erase(id);if(result.resetFailed.contains(id))Notice(L"Could not clear hardware fault file; check profile storage permissions.");else powerStatus.erase(id);}
        if(!result.resetDone.empty())InvalidateFaults();
        if(result.faultEpoch){if(result.faultEpoch==faultEpoch)faultCache=std::move(result.faults);else Count(wakeTiming.staleResults);}
        if(result.tray){if(result.trayEpoch==trayEpoch){trayOutstanding=false;nextTray=now+(result.present?30000:1000);bool previous=trayPresent;trayPresent=result.present;if(previous!=trayPresent)SetBlocked(ResetCause::Tray);}else Count(wakeTiming.staleResults);}
    }
    if(faultsDirty||now>=nextFault){
        MaintenanceRequest request;request.faultEpoch=faultEpoch;for(auto& d:displays)request.paths.emplace_back(d.id,PowerMarker(d.id));
        {std::lock_guard lock(maintenanceLock);maintenancePending.faultEpoch=request.faultEpoch;maintenancePending.paths=std::move(request.paths);maintenanceChanged.notify_one();}
        faultsDirty=false;nextFault=now+5000;
    }
    int state=controller.paused?3:controller.Any()?2:controller.config.automatic?1:0;
    if(state!=requestedTray||(!trayOutstanding&&now>=nextTray))Tray();
}
void StartMaintenance(){
    {std::lock_guard lock(maintenanceLock);maintenanceStop=false;maintenanceResultReady=false;maintenancePending={};maintenanceResult={};}
    faultCache.clear();resettingFaults.clear();faultEpoch=trayEpoch=1;nextFault=0;faultsDirty=true;trayOutstanding=false;requestedTray=-1;
    maintenanceWorker=std::thread(MaintenanceMain);
}
void StopMaintenance(){
    {std::lock_guard lock(maintenanceLock);maintenanceStop=true;maintenanceChanged.notify_one();}
    if(maintenanceWorker.joinable())maintenanceWorker.join();
    {std::lock_guard lock(maintenanceLock);if(maintenancePending.icon)DestroyIcon(maintenancePending.icon);maintenancePending={};maintenanceResult={};maintenanceResultReady=false;}
    resettingFaults.clear();trayPresent=false;
}
bool LegacyStartupPresent();
void RegisterControls() {
    UnregisterHotKey(ui,10);UnregisterHotKey(ui,11);hotkeyStatus.clear();
    if(!controller.config.hotkeys)return;
#ifdef DAC_HARNESS
    hotkeyStatus=L"Hotkeys simulated in test harness";
#else
    bool current=!!RegisterHotKey(ui,10,MOD_CONTROL|MOD_ALT|MOD_NOREPEAT,VK_F1+controller.config.currentKey-1);
    bool all=!!RegisterHotKey(ui,11,MOD_CONTROL|MOD_ALT|MOD_NOREPEAT,VK_F1+controller.config.allKey-1);
    hotkeyStatus=current&&all?L"Control hotkeys registered":L"Hotkey conflict: one or more controls unavailable";
    if(!current||!all)Notice(hotkeyStatus.c_str());
#endif
}
bool PreserveExact(const std::wstring& suffix,bool allowExisting){DWORD error=0;std::string original,backup;if(!ReadFileText(configPath,original,error,true))return false;auto path=configPath+suffix;
    if(GetFileAttributesW(Extended(path).c_str())==INVALID_FILE_ATTRIBUTES){if(!CopyFileW(Extended(configPath).c_str(),Extended(path).c_str(),TRUE))return false;}else if(!allowExisting)return false;
    return ReadFileText(path,backup,error,true)&&backup==original;
}
std::wstring saveFailure;
bool SaveFailed(const std::wstring& message,DWORD code=0){
    saveFailure=message;if(code)saveFailure+=L" (Windows error "+std::to_wstring(code)+L")";
    Record("host","settings save rejected",code);Notice(saveFailure.c_str());return false;
}
bool Save(Config next,bool reset=true,bool resetPreferences=false) {
    saveFailure.clear();
    DWORD error=0;controller.legacyAutomationBlocked=LegacyStartupPresent();
    if(next.automatic&&controller.legacyAutomationBlocked)return SaveFailed(L"Turn off Automatic to save now. Exit the predecessor DAC controller and disable any old OLED Aegis startup entry before enabling Automatic.");
    DWORD attributes=GetFileAttributesW(Extended(configPath).c_str());bool exists=attributes!=INVALID_FILE_ATTRIBUTES;
    if(!exists){DWORD code=GetLastError();if(code!=ERROR_FILE_NOT_FOUND&&code!=ERROR_PATH_NOT_FOUND)return SaveFailed(L"Cannot inspect the settings file. Check access to your local application-data folder.",code);}
    if(exists){if(resetPreferences&&!PreserveExact(L".reset-backup-"+std::to_wstring(GetTickCount64()),false))return SaveFailed(L"Could not back up existing preferences. Reset was cancelled; original settings are retained.");
        std::string previous,parseError;Config disk;bool readable=ReadFileText(configPath,previous,error)&&Parse(previous,disk,parseError);
        if(!readable&&!resetPreferences)return SaveFailed(error?L"Cannot read existing settings. Check file access; original settings are retained.":L"Existing settings are invalid or unsupported. Preserve the file before repairing it or using Reset in Advanced settings.",error);
        if(readable&&disk.sourceSchema<3&&!PreserveExact(disk.sourceSchema==1?L".v1-backup":L".v2-backup",true))return SaveFailed(L"Migration backup could not be verified. Preserve any old .v1-backup or .v2-backup separately before retrying.");
    }
    Config checked;std::string validation;if(!Parse(Serialize(next),checked,validation))return SaveFailed(L"Settings draft is invalid: "+Wide(validation)+L". Review Advanced settings before saving.");
    if(!Commit(controller.config,std::move(next),[&](const std::string& data){return WriteFileText(configPath,data,error);})) {
        Note(L"settings persistence failed",error);return SaveFailed(L"Could not write settings. Check folder permissions or file locks. Your changes remain open; previous settings are retained.",error);
    }
    if(reset){controller.effectiveReady=false;Reset();}debug=controller.config.debug;diagnostics=0;InvalidateMedia();RegisterControls();return true;
}
void Reload() {
    controller.legacyAutomationBlocked=LegacyStartupPresent();
    std::string data,error; DWORD code=0; Config next;
    if(ReadFileText(configPath,data,code)&&Parse(data,next,error)) { if(LegacyStartupPresent()) next.automatic=false; controller.config=std::move(next);controller.effectiveReady=false; debug=controller.config.debug; Reset(); InvalidateMedia();RegisterControls(); }
    else Notice(L"Cannot reload settings. Existing active settings retained; inspect the versioned configuration.");
}
bool LegacyStartupPresent() {
#ifdef DAC_HARNESS
    if(legacyStartupOverride>=0)return legacyStartupOverride!=0;
#endif
    // Read-only coexistence check for the predecessor's actual controller.
    // An installed but inactive host is not treated as a running DAC controller.
    Handle predecessor(OpenMutexW(SYNCHRONIZE,FALSE,L"Local\\DAC-Windhawk-Singleton"));if(predecessor)return true;
    HKEY key{}; if(RegOpenKeyExW(HKEY_CURRENT_USER,L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",0,KEY_QUERY_VALUE,&key)!=ERROR_SUCCESS) return false;
    LSTATUS result=RegQueryValueExW(key,L"OLED Aegis",nullptr,nullptr,nullptr,nullptr); RegCloseKey(key); return result==ERROR_SUCCESS;
}
// UI controls edit a draft. Only Save invokes the canonical checked transaction.
Config draft;std::string editorBaseline; int selection=-1;std::vector<Display> editorDisplays;
bool EditorCurrent(){return Serialize(controller.config)==editorBaseline||SaveFailed(L"Saved settings changed elsewhere. Close and reopen Advanced settings before saving; your draft has not been applied.");}
const wchar_t* StateLabel(State state) {
    switch(state){case State::Dim:return L"software dim";case State::Disabled:return L"disabled";case State::Suppressed:return L"kept awake";case State::Black:return L"black";
    case State::Launching:return L"starting saver";case State::Saver:return L"saver running";case State::Fallback:return L"black fallback";default:return L"desktop";}
}
void UpdateEditorStatus() {
    if(!editor||selection<0||selection>=static_cast<int>(editorDisplays.size()))return;
    auto& d=editorDisplays[selection];auto n=controller.nodes.find(d.id);std::wstring text=d.identified?L"State: ":L"Unidentified output; protection unavailable. ";
    if(n!=controller.nodes.end())text+=ExplanationText(d.id);else text+=L"Disconnected (draft retained)";
    if(n!=controller.nodes.end()&&n->second.sticky)text+=L" (sticky)";
    if(powerStatus.contains(d.id))text+=L"\n"+powerStatus[d.id];else if(CachedFault(d.id))text+=L"\nHardware fault state: "+Wide(FaultCacheState(d.id))+L"; confirm physical wake before reset.";
    SetDlgItemTextW(editor,147,text.c_str());
}
bool ValidMonitorNumbers() {
    if(selection<0)return true;
    for(int id:{140,143,145,150,151,152,155,156}){BOOL valid=false;GetDlgItemInt(editor,id,&valid,FALSE);if(!valid)return false;}return true;
}
void StoreMonitorDraft() {
    if(selection<0||selection>=static_cast<int>(editorDisplays.size())) return;
    auto& p=draft.monitors[editorDisplays[selection].id];
    p.enabled=IsDlgButtonChecked(editor,121)==BST_CHECKED;
    p.saver=static_cast<int>(SendDlgItemMessageW(editor,122,CB_GETCURSEL,0,0))-1;
    p.timeout=GetDlgItemInt(editor,140,nullptr,FALSE);p.input=static_cast<int>(SendDlgItemMessageW(editor,141,CB_GETCURSEL,0,0))-1;
    p.media=static_cast<int>(SendDlgItemMessageW(editor,142,CB_GETCURSEL,0,0))-1;p.blackAfter=GetDlgItemInt(editor,143,nullptr,FALSE);
    p.hardware=IsDlgButtonChecked(editor,144)==BST_CHECKED;p.powerAfter=GetDlgItemInt(editor,145,nullptr,FALSE);
    p.fullscreen=IsDlgButtonChecked(editor,146)==BST_CHECKED;
    p.dim=GetDlgItemInt(editor,150,nullptr,FALSE);p.fadeMs=GetDlgItemInt(editor,151,nullptr,FALSE);p.dimSeconds=GetDlgItemInt(editor,152,nullptr,FALSE);p.batteryBlack=IsDlgButtonChecked(editor,153)==BST_CHECKED;
    p.sceneTheme=static_cast<int>(SendDlgItemMessageW(editor,154,CB_GETCURSEL,0,0));p.sceneSeconds=GetDlgItemInt(editor,155,nullptr,FALSE);p.rotateSeconds=GetDlgItemInt(editor,156,nullptr,FALSE);
    draft.controllerInput=IsDlgButtonChecked(editor,157)==BST_CHECKED;
}
void LoadMonitorDraft() {
    selection=static_cast<int>(SendDlgItemMessageW(editor,120,CB_GETCURSEL,0,0));
    if(selection<0||selection>=static_cast<int>(editorDisplays.size())) return;
    auto& d=editorDisplays[selection]; auto it=draft.monitors.find(d.id); Preference p=it==draft.monitors.end()?Preference{}:it->second;
    CheckDlgButton(editor,121,p.enabled?BST_CHECKED:BST_UNCHECKED); EnableWindow(GetDlgItem(editor,121),d.identified);
    SendDlgItemMessageW(editor,122,CB_SETCURSEL,p.saver+1,0);
    SetDlgItemInt(editor,140,p.timeout,FALSE);SetDlgItemInt(editor,143,p.blackAfter,FALSE);SetDlgItemInt(editor,145,p.powerAfter,FALSE);
    SendDlgItemMessageW(editor,141,CB_SETCURSEL,p.input+1,0);SendDlgItemMessageW(editor,142,CB_SETCURSEL,p.media+1,0);
    CheckDlgButton(editor,144,p.hardware?BST_CHECKED:BST_UNCHECKED);CheckDlgButton(editor,146,p.fullscreen?BST_CHECKED:BST_UNCHECKED);
    SetDlgItemInt(editor,150,p.dim,FALSE);SetDlgItemInt(editor,151,p.fadeMs,FALSE);SetDlgItemInt(editor,152,p.dimSeconds,FALSE);SetDlgItemInt(editor,155,p.sceneSeconds,FALSE);SetDlgItemInt(editor,156,p.rotateSeconds,FALSE);
    CheckDlgButton(editor,153,p.batteryBlack?BST_CHECKED:BST_UNCHECKED);SendDlgItemMessageW(editor,154,CB_SETCURSEL,p.sceneTheme,0);CheckDlgButton(editor,157,draft.controllerInput?BST_CHECKED:BST_UNCHECKED);
    EnableWindow(GetDlgItem(editor,144),d.identified);UpdateEditorStatus();
}
void PopulateEditor(bool fromLive=true) {
    if(fromLive){draft=controller.config;editorBaseline=Serialize(controller.config);}editorDisplays=displays;
    SetDlgItemInt(editor,101,draft.timeout,FALSE); SetDlgItemInt(editor,102,draft.poll,FALSE); SetDlgItemInt(editor,103,draft.padding,FALSE);
    bool flags[]={draft.automatic,draft.perInput,draft.media,draft.perMedia,draft.muted,draft.debug};
    for(int i=0;i<6;++i) CheckDlgButton(editor,110+i,flags[i]?BST_CHECKED:BST_UNCHECKED);
    SendDlgItemMessageW(editor,120,CB_RESETCONTENT,0,0);
    for(auto& d:editorDisplays) SendDlgItemMessageW(editor,120,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(d.label.c_str()));
    SendDlgItemMessageW(editor,120,CB_SETCURSEL,0,0); LoadMonitorDraft();
}
void ImportLegacy(HWND w) {
    std::vector<wchar_t> path(32768); OPENFILENAMEW ofn{}; ofn.lStructSize=sizeof(ofn); ofn.hwndOwner=w;
    ofn.lpstrFilter=L"Legacy INI\0*.ini\0All files\0*.*\0"; ofn.lpstrFile=path.data(); ofn.nMaxFile=32768;
    ofn.Flags=OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR; ofn.lpstrTitle=L"Import legacy OLED Aegis INI (original remains unchanged)";
    if(!GetOpenFileNameW(&ofn)) return;
    std::string text,error; DWORD code=0; int ignored=0; Config next=controller.config;
    std::vector<std::pair<std::string,std::string>> aliases;
    for(auto& d:displays) aliases.push_back({d.id,d.alias});
    if(!ReadFileText(path.data(),text,code)||!Import(text,aliases,next,ignored,error)) { MessageBoxW(w,L"Legacy file could not be read or validated. No changes made.",L"Import failed",MB_OK|MB_ICONERROR); return; }
    if(LegacyStartupPresent()) next.automatic=false;
    if(Save(next)) {
        PopulateEditor(); auto message=L"Import saved; original INI unchanged. Ignored keys: "+std::to_wstring(ignored)+
            L".\nOld numeric/display-name aliases use the current display order.\nAutomatic activation and hardware power were disabled for every imported policy. Review the settings before enabling them explicitly. Exit any predecessor controller before enabling Automatic. DAC login startup is managed from Settings in the tray menu.";
        MessageBoxW(w,message.c_str(),L"Import complete",MB_OK);
    }
}
std::wstring ControlText(HWND w,int id) {wchar_t text[4096]{};GetDlgItemTextW(w,id,text,4096);return text;}
struct SettingsControl {HWND window;int x,y,width,height;};
struct SettingsLayout {int dpi=96;std::vector<SettingsControl> controls;};
SettingsLayout editorLayout,optionsLayout,workspaceLayout,setupLayout;
void RefreshSetupTypography();
void LayoutSetup(HWND w);
void LayoutSettings(HWND w,SettingsLayout& layout,int height,int& position) {
    RECT client{};GetClientRect(w,&client);SCROLLINFO info{};info.cbSize=sizeof(info);info.fMask=SIF_RANGE|SIF_PAGE|SIF_POS;
    info.nMax=MulDiv(height,layout.dpi,96);info.nPage=std::max<LONG>(0,client.bottom);info.nPos=position;SetScrollInfo(w,SB_VERT,&info,TRUE);
    info.fMask=SIF_POS;GetScrollInfo(w,SB_VERT,&info);position=info.nPos;
    for(auto& c:layout.controls){
        wchar_t cls[32]{};GetClassNameW(c.window,cls,32);if(wcscmp(cls,L"ComboBox")==0){SendMessageW(c.window,CB_SETITEMHEIGHT,0,MulDiv(24,layout.dpi,96));SendMessageW(c.window,CB_SETITEMHEIGHT,static_cast<WPARAM>(-1),MulDiv(24,layout.dpi,96));}
        SetWindowPos(c.window,nullptr,MulDiv(c.x,layout.dpi,96),MulDiv(c.y,layout.dpi,96)-position,
        MulDiv(c.width,layout.dpi,96),MulDiv(c.height,layout.dpi,96),SWP_NOZORDER|SWP_NOACTIVATE);
    }
}
void RefreshSettingsTheme() {
    ReadUiTheme();
    for(auto window:{editor,options,workspace,setupWindow})if(window){
        auto& layout=window==editor?editorLayout:window==options?optionsLayout:window==workspace?workspaceLayout:setupLayout;auto& font=window==editor?editorFont:window==options?optionsFont:window==workspace?workspaceFont:setupFont;
        HFONT replacement=SettingsFont(layout.dpi,window==setupWindow?18:0);if(replacement){for(auto& c:layout.controls)SendMessageW(c.window,WM_SETFONT,reinterpret_cast<WPARAM>(replacement),FALSE);if(font)DeleteObject(font);font=replacement;}
        if(window==setupWindow){RefreshSetupTypography();LayoutSetup(window);}
        ThemeChrome(window);RedrawWindow(window,nullptr,nullptr,RDW_INVALIDATE|RDW_ERASE|RDW_ALLCHILDREN);
    }
}
void SettingsDpiChanged(HWND w,WPARAM wp,LPARAM lp,SettingsLayout& layout,HFONT& font,int height,int& position) {
    int dpi=HIWORD(wp);if(dpi<=0||!lp)return;
    position=MulDiv(position,dpi,layout.dpi);layout.dpi=dpi;
    HFONT replacement=SettingsFont(dpi,w==setupWindow?18:0);
    if(replacement){for(auto& c:layout.controls)SendMessageW(c.window,WM_SETFONT,reinterpret_cast<WPARAM>(replacement),TRUE);if(font)DeleteObject(font);font=replacement;}
    if(w==setupWindow)RefreshSetupTypography();
    RECT bounds=*reinterpret_cast<const RECT*>(lp);MONITORINFO monitor{};monitor.cbSize=sizeof(monitor);
    if(GetMonitorInfoW(MonitorFromRect(&bounds,MONITOR_DEFAULTTONEAREST),&monitor)) {
        int available=monitor.rcWork.bottom-monitor.rcWork.top;
        if(bounds.bottom-bounds.top>available){bounds.top=monitor.rcWork.top;bounds.bottom=monitor.rcWork.bottom;}
    }
    SetWindowPos(w,nullptr,bounds.left,bounds.top,bounds.right-bounds.left,bounds.bottom-bounds.top,SWP_NOZORDER|SWP_NOACTIVATE);
    LayoutSettings(w,layout,height,position);RedrawWindow(w,nullptr,nullptr,RDW_INVALIDATE|RDW_ERASE|RDW_ALLCHILDREN);
}
bool ScrollSettings(HWND w,UINT msg,WPARAM wp,LPARAM lp,int height,int& position,SettingsLayout& layout) {
    (void)lp;
    if(msg==WM_SIZE){LayoutSettings(w,layout,height,position);return true;}
    if(msg!=WM_VSCROLL&&msg!=WM_MOUSEWHEEL)return false;
    SCROLLINFO info{};info.cbSize=sizeof(info);info.fMask=SIF_ALL;GetScrollInfo(w,SB_VERT,&info);int pos=info.nPos;
    if(msg==WM_MOUSEWHEEL)pos-=GET_WHEEL_DELTA_WPARAM(wp)/WHEEL_DELTA*64;
    else switch(LOWORD(wp)){case SB_LINEUP:pos-=32;break;case SB_LINEDOWN:pos+=32;break;case SB_PAGEUP:pos-=info.nPage;break;case SB_PAGEDOWN:pos+=info.nPage;break;case SB_THUMBTRACK:pos=info.nTrackPos;break;}
    info.fMask=SIF_POS;info.nPos=pos;SetScrollInfo(w,SB_VERT,&info,TRUE);GetScrollInfo(w,SB_VERT,&info);ScrollWindowEx(w,0,position-info.nPos,nullptr,nullptr,nullptr,nullptr,SW_SCROLLCHILDREN|SW_INVALIDATE|SW_ERASE);position=info.nPos;return true;
}
void FitSettings(HWND w,int width,int height) {
    MONITORINFO monitor{};monitor.cbSize=sizeof(monitor);GetMonitorInfoW(MonitorFromWindow(w,MONITOR_DEFAULTTONEAREST),&monitor);int dpi=GetDpiForWindow(w);
    SetWindowPos(w,nullptr,monitor.rcWork.left,monitor.rcWork.top,MulDiv(width,dpi,96),std::min<int>(MulDiv(height,dpi,96),monitor.rcWork.bottom-monitor.rcWork.top),SWP_NOZORDER|SWP_NOACTIVATE);
}
int optionsScroll=0;
LRESULT CALLBACK OptionsProc(HWND w,UINT msg,WPARAM wp,LPARAM lp) {
    LRESULT themed{};if(ThemeMessage(w,msg,wp,lp,themed))return themed;
    if(ScrollSettings(w,msg,wp,lp,576,optionsScroll,optionsLayout))return 0;
    if(msg==WM_COMMAND) {
        int id=LOWORD(wp);
        if(id==203){std::vector<wchar_t> path(32768);OPENFILENAMEW dialog{};dialog.lStructSize=sizeof(dialog);dialog.hwndOwner=w;dialog.lpstrFile=path.data();dialog.nMaxFile=32768;dialog.lpstrFilter=L"Installed Windows savers\0*.scr\0";dialog.Flags=OFN_FILEMUSTEXIST|OFN_NOCHANGEDIR;
            if(GetOpenFileNameW(&dialog))SetDlgItemTextW(w,201,path.data());}
        else if(id==204){std::vector<wchar_t> path(32768);BROWSEINFOW browse{};browse.hwndOwner=w;browse.lpszTitle=L"Choose this monitor's photo folder";browse.ulFlags=BIF_RETURNONLYFSDIRS|BIF_NEWDIALOGSTYLE;
            auto item=SHBrowseForFolderW(&browse);if(item){if(SHGetPathFromIDListEx(item,path.data(),32768,GPFIDL_DEFAULT))SetDlgItemTextW(w,202,path.data());CoTaskMemFree(item);}}
        else if(id==212){CHOOSECOLORW color{};COLORREF custom[16]{};color.lStructSize=sizeof(color);color.hwndOwner=w;color.lpCustColors=custom;color.Flags=CC_FULLOPEN|CC_RGBINIT;
            int rgb=0;Number(Utf8(ControlText(w,213)),rgb);color.rgbResult=RGB((rgb>>16)&255,(rgb>>8)&255,rgb&255);
            if(ChooseColorW(&color))SetDlgItemInt(w,213,(GetRValue(color.rgbResult)<<16)|(GetGValue(color.rgbResult)<<8)|GetBValue(color.rgbResult),FALSE);}
        else if(id==220&&selection>=0&&selection<static_cast<int>(editorDisplays.size())) {
            BOOL a,b,c,d;int interval=GetDlgItemInt(w,205,&a,FALSE),current=GetDlgItemInt(w,209,&b,FALSE),all=GetDlgItemInt(w,210,&c,FALSE),color=GetDlgItemInt(w,213,&d,FALSE);
            if(!a||!b||!c||!d||interval<5||interval>3600||current<1||current>12||all<1||all>12||current==all||color<0||color>0xffffff){MessageBoxW(w,L"Use 5–3600 seconds, distinct F1–F12 hotkeys and a valid background color.",L"Invalid options",MB_OK);return 0;}
            auto& p=draft.monitors[editorDisplays[selection].id];auto path=ControlText(w,201),folder=ControlText(w,202);
            if(!path.empty()&&!LocalFile(path,L".scr")){MessageBoxW(w,L"Select an existing .scr file on a drive. Savers run as programs: choose a trusted installed saver.",L"Invalid saver",MB_OK);return 0;}
            if(!folder.empty()){DWORD attrs=GetFileAttributesW(Extended(folder).c_str());if(folder.size()<3||folder[1]!=L':'||attrs==INVALID_FILE_ATTRIBUTES||!(attrs&FILE_ATTRIBUTE_DIRECTORY)||(attrs&FILE_ATTRIBUTE_REPARSE_POINT)){MessageBoxW(w,L"Choose an accessible folder on a drive, without a junction or symbolic link.",L"Invalid photo folder",MB_OK);return 0;}}
            p.custom=Utf8(path);p.folder=Utf8(folder);p.slideSeconds=interval;p.placement=static_cast<int>(SendDlgItemMessageW(w,206,CB_GETCURSEL,0,0));p.shuffle=IsDlgButtonChecked(w,207)==BST_CHECKED;p.recursive=IsDlgButtonChecked(w,208)==BST_CHECKED;p.background=color;
            draft.hotkeys=IsDlgButtonChecked(w,211)==BST_CHECKED;draft.currentKey=current;draft.allKey=all;DestroyWindow(w);
        }else if(id==221||id==IDCANCEL)DestroyWindow(w);return 0;
    }
    if(msg==WM_CLOSE){DestroyWindow(w);return 0;}
    if(msg==WM_DPICHANGED){SettingsDpiChanged(w,wp,lp,optionsLayout,optionsFont,576,optionsScroll);return 0;}
    if(msg==WM_DESTROY){options=nullptr;if(editor){EnableWindow(editor,TRUE);SetForegroundWindow(editor);}return 0;}
    if(msg==WM_NCDESTROY){optionsLayout.controls.clear();if(optionsFont){DeleteObject(optionsFont);optionsFont=nullptr;}}
    return DefWindowProcW(w,msg,wp,lp);
}
void ShowSaverOptions() {
    if(options||selection<0||selection>=static_cast<int>(editorDisplays.size()))return;StoreMonitorDraft();
    auto& p=draft.monitors[editorDisplays[selection].id];int dpi=GetDpiForWindow(editor);optionsLayout={};optionsLayout.dpi=dpi;auto px=[&](int value){return MulDiv(value,dpi,96);};
    optionsScroll=0;options=CreateWindowExW(WS_EX_DLGMODALFRAME,kOptions,L"DAC — Display Activity Controls — Saver files, slideshow and hotkeys",WS_CAPTION|WS_SYSMENU|WS_VSCROLL,CW_USEDEFAULT,CW_USEDEFAULT,px(720),px(620),editor,nullptr,GetModuleHandleW(nullptr),nullptr);if(!options)return;FitSettings(options,720,620);
    dpi=GetDpiForWindow(options);optionsLayout.dpi=dpi;
    optionsFont=SettingsFont(dpi);ThemeChrome(options);
    auto control=[&](const wchar_t* cls,const wchar_t* text,DWORD style,int x,int y,int width,int height,int id){auto child=CreateWindowExW(0,cls,text,WS_CHILD|WS_VISIBLE|style,px(x),px(y),px(width),px(height),options,reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),GetModuleHandleW(nullptr),nullptr);optionsLayout.controls.push_back({child,x,y,width,height});ThemeControl(child,cls);SendMessageW(child,WM_SETFONT,reinterpret_cast<WPARAM>(optionsFont?optionsFont:GetStockObject(DEFAULT_GUI_FONT)),TRUE);return child;};
    control(L"STATIC",L"Local saver program (.scr); select Custom in the monitor assignment",0,16,16,655,24,0);
    control(L"EDIT",Wide(p.custom).c_str(),WS_BORDER|ES_AUTOHSCROLL|WS_TABSTOP,16,44,525,26,201);control(L"BUTTON",L"Browse…",BS_PUSHBUTTON|WS_TABSTOP,555,44,110,26,203);
    control(L"STATIC",L"Independent photo folder; select Photo slideshow in the assignment",0,16,86,655,24,0);
    control(L"EDIT",Wide(p.folder).c_str(),WS_BORDER|ES_AUTOHSCROLL|WS_TABSTOP,16,114,525,26,202);control(L"BUTTON",L"Browse…",BS_PUSHBUTTON|WS_TABSTOP,555,114,110,26,204);
    control(L"STATIC",L"Seconds per photo (5–3600)       Placement",0,16,156,650,24,0);
    control(L"EDIT",L"",WS_BORDER|ES_NUMBER|WS_TABSTOP,16,184,200,26,205);SetDlgItemInt(options,205,p.slideSeconds,FALSE);
    control(L"COMBOBOX",L"",CBS_DROPDOWNLIST|CBS_OWNERDRAWFIXED|CBS_HASSTRINGS|WS_VSCROLL|WS_TABSTOP,240,184,425,200,206);
    for(auto label:{L"Fit",L"Fill (crop)",L"Stretch",L"Center at original size",L"Tile",L"Fit without enlarging"})SendDlgItemMessageW(options,206,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(label));SendDlgItemMessageW(options,206,CB_SETCURSEL,p.placement,0);
    control(L"BUTTON",L"Shuffle playlist",BS_AUTOCHECKBOX|WS_TABSTOP,16,225,260,26,207);CheckDlgButton(options,207,p.shuffle?BST_CHECKED:BST_UNCHECKED);
    control(L"BUTTON",L"Include subfolders",BS_AUTOCHECKBOX|WS_TABSTOP,310,225,320,26,208);CheckDlgButton(options,208,p.recursive?BST_CHECKED:BST_UNCHECKED);
    control(L"BUTTON",L"Background color…",BS_PUSHBUTTON|WS_TABSTOP,16,264,240,28,212);control(L"EDIT",L"",WS_BORDER|ES_NUMBER|WS_TABSTOP,280,264,210,28,213);SetDlgItemInt(options,213,p.background,FALSE);
    control(L"BUTTON",L"Enable Ctrl+Alt+F-key sticky toggle shortcuts (all monitors)",BS_AUTOCHECKBOX|WS_TABSTOP,16,310,650,28,211);CheckDlgButton(options,211,draft.hotkeys?BST_CHECKED:BST_UNCHECKED);
    control(L"STATIC",L"Current monitor F-number          All enabled monitors F-number",0,16,352,650,24,0);
    control(L"EDIT",L"",WS_BORDER|ES_NUMBER|WS_TABSTOP,16,380,240,26,209);SetDlgItemInt(options,209,draft.currentKey,FALSE);
    control(L"EDIT",L"",WS_BORDER|ES_NUMBER|WS_TABSTOP,310,380,240,26,210);SetDlgItemInt(options,210,draft.allKey,FALSE);
    auto text=L"Hotkeys toggle sticky mode; pointer selects the current monitor. Emergency exit remains Ctrl+Alt+Shift+F12.\nPhotos use JPEG, PNG, BMP or GIF (first frame); bounded to 2000 files.\n"+hotkeyStatus;
    control(L"STATIC",text.c_str(),0,16,430,650,76,0);
    control(L"BUTTON",L"Apply to settings draft",BS_PUSHBUTTON|WS_TABSTOP,16,530,330,32,220);control(L"BUTTON",L"Cancel",BS_PUSHBUTTON|WS_TABSTOP,380,530,280,32,221);
    LayoutSettings(options,optionsLayout,576,optionsScroll);EnableWindow(editor,FALSE);
#ifndef DAC_HARNESS
    ShowWindow(options,SW_SHOWNORMAL);
#endif
}
const wchar_t* kWorkspace=L"DAC-Workspace";
int workspaceScroll=0,setupScroll=0;
Config pendingImport;bool importPending=false;std::vector<AppRule> workspaceRules;std::map<std::string,std::string> pendingMapping;std::vector<std::string> importSources;std::vector<Display> importTargets;
std::vector<std::pair<HWND,Time>> identifyWindows;std::map<HWND,std::wstring> identifyLabels;
bool StoreGlobalDraft(){BOOL a,b,c;draft.timeout=GetDlgItemInt(editor,101,&a,FALSE);draft.poll=GetDlgItemInt(editor,102,&b,FALSE);draft.padding=GetDlgItemInt(editor,103,&c,FALSE);if(!a||!b||!c||!ValidMonitorNumbers())return false;StoreMonitorDraft();bool* flags[]={&draft.automatic,&draft.perInput,&draft.media,&draft.perMedia,&draft.muted,&draft.debug};for(int i=0;i<6;++i)*flags[i]=IsDlgButtonChecked(editor,110+i)==BST_CHECKED;return true;}
std::string RuleLines(const Policy& policy){std::string text;for(auto& r:policy.rules)text+=(r.kind?"A|":"F|")+std::string(r.global?"G|":"D|")+(r.nameOnly?"N|":"P|")+r.executable+"\r\n";return text;}
bool ReadRules(const std::string& text,Policy& policy){std::vector<AppRule> rules;std::istringstream in(text);std::string line;while(std::getline(in,line)){line=Trim(line);if(line.empty())continue;if(line.size()<7||line[1]!='|'||line[3]!='|'||line[5]!='|'||(line[0]!='F'&&line[0]!='A')||(line[2]!='G'&&line[2]!='D')||(line[4]!='P'&&line[4]!='N'))return false;rules.push_back({line[0]=='A',line[2]=='G',line[4]=='N',line.substr(6)});}Config check;static_cast<Policy&>(check)=policy;check.rules=rules;Config parsed;std::string error;if(!Parse(Serialize(check),parsed,error))return false;policy.rules=parsed.rules;return true;}
bool BackupPreferences(){DWORD error=0;std::string bytes;if(!ReadFileText(configPath,bytes,error)){if(error==ERROR_FILE_NOT_FOUND||error==ERROR_PATH_NOT_FOUND)return true;return false;}return WriteFileText(configPath+L".reset-backup-"+std::to_wstring(GetTickCount64()),bytes,error);}
Config Portable(const Policy& policy){Config out;static_cast<Policy&>(out)=policy;out.automatic=false;for(auto& [id,p]:out.monitors){(void)id;p.hardware=false;}return out;}
bool MapImported(const Policy& source,const std::map<std::string,std::string>& mapping,Policy& destination){Policy next=source;next.monitors.clear();next.automatic=false;std::set<std::string> targets;
    if(mapping.size()!=source.monitors.size())return false;for(auto& [id,p]:source.monitors){auto m=mapping.find(id);if(m==mapping.end()||m->second.empty()||!targets.insert(m->second).second)return false;auto copy=p;copy.hardware=false;next.monitors[m->second]=copy;}destination=std::move(next);return true;}
std::wstring SelectLocalFile(HWND owner,bool save,const wchar_t* title){std::vector<wchar_t> path(32768);OPENFILENAMEW d{};d.lStructSize=sizeof(d);d.hwndOwner=owner;d.lpstrFile=path.data();d.nMaxFile=32768;d.lpstrFilter=L"Local profile or diagnostics text\0*.ini;*.txt\0All files\0*.*\0";d.lpstrTitle=title;d.Flags=OFN_NOCHANGEDIR|OFN_PATHMUSTEXIST|(save?OFN_OVERWRITEPROMPT:OFN_FILEMUSTEXIST);if(save?GetSaveFileNameW(&d):GetOpenFileNameW(&d))return path.data();return {};}
std::wstring ConflictText(){BOOL enabled=FALSE;UINT timeout=0;SetLastError(0);bool a=!!SystemParametersInfoW(SPI_GETSCREENSAVEACTIVE,0,&enabled,0);DWORD ae=a?0:GetLastError();SetLastError(0);bool b=!!SystemParametersInfoW(SPI_GETSCREENSAVETIMEOUT,0,&timeout,0);DWORD be=b?0:GetLastError();
    std::wstring result=L"Windows screensaver: "+(a?std::wstring(enabled?L"enabled":L"disabled"):L"query unavailable ("+std::to_wstring(ae)+L")")+L"\nTimeout: "+(b?std::to_wstring(timeout)+L" seconds":L"unavailable ("+std::to_wstring(be)+L")");
    Handle snapshot(CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0));unsigned count=0;PROCESSENTRY32W entry{};entry.dwSize=sizeof(entry);bool processQuery=snapshot&&Process32FirstW(snapshot,&entry);if(processQuery)do{for(auto name:{L"oled_aegis.exe",L"idledimmer.exe",L"oled-sleeper.exe",L"lively.exe"})if(_wcsicmp(entry.szExeFile,name)==0)++count;}while(Process32NextW(snapshot,&entry));
    result+=L"\nKnown controller names observed: "+(processQuery?std::to_wstring(count):L"query unavailable")+L" (advisory, not exhaustive).\nNo OS setting or process has been changed. Windows policy may be managed.\nUse Windows Settings > Personalization > Lock screen > Screen saver for details.";return result;}
std::string SafeDiagnosticEvent(const std::string& event){for(unsigned i=0;i<=static_cast<unsigned>(Reason::HardwareFault);++i)if(event==ReasonText(static_cast<Reason>(i)))return event;for(auto allowed:{"saver fallback","owned process started","owned process exit wait expired","too many stopping sessions","dim alpha unavailable","worker creation failed","configuration failed","settings persistence failed","settings save rejected","hardware off accepted","hardware unavailable","hardware wake failed","hardware wake accepted"})if(event==allowed)return event;return "event redacted";}
std::string SafeDiagnosticAlias(const std::string& alias){int number=0;if(alias.starts_with("Display ")&&Number(alias.substr(8),number)&&number>0&&number<=64)return alias;return "host";}
std::string DiagnosticText(){std::ostringstream out;out<<"DAC — Display Activity Controls "<<kVersion<<"\nRedacted local diagnostics; no paths, identities, titles or recovery secrets.\n";
    auto& policy=controller.PolicyNow();out<<"automatic="<<policy.automatic<<" timeout="<<policy.timeout<<" poll="<<policy.poll<<" controllerInput="<<policy.controllerInput<<" profileActive="<<!controller.activeProfile.empty()<<"\n";
    for(size_t i=0;i<displays.size();++i){auto& d=displays[i];auto p=controller.Pref(d.id);out<<"Display "<<i+1<<": enabled="<<p.enabled<<" saver="<<p.saver<<" dimOpacity="<<p.dim<<" inputMode="<<p.input<<" hardwareOptIn="<<p.hardware<<" faultState="<<FaultCacheState(d.id)<<"\n";}
    out<<"Wake timing maxima (queue/heartbeat ms, execution us; no physical render timing): queue="<<wakeTiming.queueMaxMs<<" heartbeat="<<wakeTiming.heartbeatMaxMs<<" input="<<wakeTiming.inputMaxUs<<" reconcile="<<wakeTiming.reconcileMaxUs<<" timer="<<wakeTiming.timerMaxUs<<" file="<<wakeTiming.fileMaxUs<<" tray="<<wakeTiming.trayMaxUs<<"\n";
    out<<"Wake counters: packets="<<wakeTiming.inputs<<" acceptedTargets="<<wakeTiming.accepted<<" invalidTimestamp="<<wakeTiming.invalidTime<<" sessionTopologyBoundary="<<wakeTiming.boundaryRejected<<" unknownScope="<<wakeTiming.unattributed<<" sticky="<<wakeTiming.stickyRejected<<" disabledPausedBlocked="<<wakeTiming.inhibited<<" historicalFocusUnavailable="<<wakeTiming.focusUnavailable<<" readFailure="<<wakeTiming.readFailures<<" staleMaintenance="<<wakeTiming.staleResults<<" fileFailure="<<wakeTiming.fileFailures<<" trayCalls="<<wakeTiming.trayCalls<<"\n";
    out<<"Wake stages: wmInput="<<wakeTiming.receipts<<" rawCalls="<<wakeTiming.rawCalls<<" rawFailure="<<wakeTiming.rawFailure<<" fallbackReadFailure="<<wakeTiming.fallbackReadFailure<<" rawSuccess="<<wakeTiming.readSuccess<<" unsupported="<<wakeTiming.unsupported<<" validTimestamp="<<wakeTiming.validTime<<" directMetadata="<<wakeTiming.directMetadata<<" nestedMetadata="<<wakeTiming.nestedMetadata<<" topologyPending="<<wakeTiming.topologyRejected<<" timestampBoundary="<<wakeTiming.timestampBoundary<<"\n";
    out<<"Wake attribution: pointerAbsent="<<wakeTiming.pointerAbsent<<" pointerUnique="<<wakeTiming.pointerUnique<<" pointerAmbiguous="<<wakeTiming.pointerAmbiguous<<" foregroundFresh="<<wakeTiming.foregroundFresh<<" foregroundUnavailable="<<wakeTiming.foregroundUnavailable<<"\n";
    out<<"Wake policy: desktopAccepted="<<controller.inputDiagnostics.desktopAccepted<<" activeAccepted="<<controller.inputDiagnostics.activeAccepted<<" activeToDesktop="<<controller.inputDiagnostics.activeToDesktop<<"\n";
    out<<"Wake hiding (window-state observations, compositor disappearance unmeasured): reconcile="<<wakeTiming.reconcileEntries<<" newlyCancelled="<<wakeTiming.newlyCancelled<<" newlyCancelledPower="<<wakeTiming.newlyCancelledPower<<" requests="<<wakeTiming.hideRequests<<" preVisible="<<wakeTiming.preVisible<<" preHidden="<<wakeTiming.preHidden<<" preInvalid="<<wakeTiming.preInvalid<<" postVisible="<<wakeTiming.postVisible<<" postHidden="<<wakeTiming.postHidden<<" postInvalid="<<wakeTiming.postInvalid<<"\n";
    out<<"UI workload: heartbeatSamples="<<wakeTiming.heartbeatSamples<<" paintSamples="<<wakeTiming.paintSamples<<" paintMaxUs="<<wakeTiming.paintMaxUs<<" paintPixelsMax="<<wakeTiming.paintPixelsMax<<" blackPaints="<<wakeTiming.blackPaints<<" clockPaints="<<wakeTiming.clockPaints<<" constellationPaints="<<wakeTiming.constellationPaints<<" emptyCoverage="<<wakeTiming.coverageEmpty<<" partialCoverage="<<wakeTiming.coveragePartial<<" fullCoverage="<<wakeTiming.coverageFull<<" counterBytes="<<sizeof(wakeTiming)+sizeof(controller.inputDiagnostics)<<"\n";
    out<<"Reset causes (other,session,power,tray,topology):";for(auto& count:wakeTiming.resets)out<<' '<<count.load();out<<"\n";
    out<<"Coverage bins (0,1,2,3,4-7,8-15,16-31,32+ active nodes):";for(auto& count:wakeTiming.coverage)out<<' '<<count.load();out<<"\n";
    auto histogram=[&](const char* label,const TimingBins& bins){out<<label<<" bins (<=1,4,16,64,256,1024,4096,overflow):";for(auto& count:bins.bins)out<<' '<<count.load();out<<"\n";};
    histogram("raw us",wakeTiming.rawUs);histogram("dispatch us",wakeTiming.dispatchUs);histogram("paint us",wakeTiming.paintUs);histogram("timer us",wakeTiming.timerUs);histogram("heartbeat ms",wakeTiming.heartbeatMs);histogram("queue ms",wakeTiming.queueMs);
    AdapterSnapshot sample;{std::lock_guard lock(observationLock);sample=adapters;}out<<"powerSource="<<static_cast<int>(sample.power)<<" xinputAvailable="<<sample.controllerAvailable<<" controllerConnected="<<sample.connected<<"\n";
    {std::lock_guard lock(diagnosticLock);for(auto& e:diagnosticEvents)out<<e.at<<' '<<SafeDiagnosticAlias(e.alias)<<' '<<SafeDiagnosticEvent(e.event)<<" code="<<e.code<<'\n';}return out.str();}
void ExportDiagnostics(HWND owner){auto path=SelectLocalFile(owner,true,L"Export redacted diagnostics locally");if(path.empty())return;DWORD error=0;if(!WriteFileText(path,DiagnosticText(),error))Notice(L"Diagnostics export failed; choose a writable local file.");else Notice(L"Redacted diagnostics saved locally.");}
void ExportSettings(HWND owner) {
    auto path=SelectLocalFile(owner,true,L"Export all protection settings (contains configured paths)");if(path.empty())return;
    DWORD error=0;if(!WriteFileText(path,Serialize(controller.config),error))Notice(L"Settings export failed; choose a writable local file.");
    else Notice(L"Protection settings exported. Appearance, tray preferences and Windows login registration remain local. Imported automation and hardware permissions will be disabled.");
}
void ImportSettings(HWND owner) {
    auto path=SelectLocalFile(owner,false,L"Import all protection settings");if(path.empty())return;
    std::string text,error;DWORD code=0;Config next;
    if(!ReadFileText(path,text,code)||!Parse(text,next,error)){Notice(L"The settings file is unreadable, malformed or unsupported. Current settings are retained.");return;}
    DisarmImportedConfig(next);
    if(MessageBoxW(owner,L"Replace protection settings with this file?\n\nCurrent presentations will stop. Automatic activation and hardware power will be disabled in every imported policy. Display identities will be retained; disconnected displays remain inactive.\n\nExisting settings will be backed up before replacement. Appearance, tray preferences and Windows login registration are unchanged. Open settings drafts will require reopening before they can be saved.\n\nReview the imported configuration before explicitly enabling automation or hardware power.",L"Review settings import",MB_YESNO|MB_DEFBUTTON2|MB_ICONQUESTION)!=IDYES)return;
    if(Save(std::move(next),true,true))Notice(L"Settings imported with automatic activation and hardware power disabled. Reopen settings to review the imported values before enabling protection.");
}
void IdentifyDisplays(){for(auto& item:identifyWindows){{std::lock_guard lock(registryLock);ownedWindows.erase(item.first);}identifyLabels.erase(item.first);DestroyWindow(item.first);}identifyWindows.clear();for(auto& d:displays){int dpi=96;int width=MulDiv(360,dpi,96),height=120;HWND window=CreateWindowExW(WS_EX_TOPMOST|WS_EX_NOACTIVATE|WS_EX_TOOLWINDOW|WS_EX_LAYERED|WS_EX_TRANSPARENT,kClass,L"Identify display",WS_POPUP,d.rect.left+(d.rect.right-d.rect.left-width)/2,d.rect.top+(d.rect.bottom-d.rect.top-height)/2,width,height,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);if(!window)continue;dpi=static_cast<int>(GetDpiForWindow(window));width=MulDiv(360,dpi,96);height=MulDiv(120,dpi,96);SetWindowPos(window,nullptr,d.rect.left+(d.rect.right-d.rect.left-width)/2,d.rect.top+(d.rect.bottom-d.rect.top-height)/2,width,height,SWP_NOACTIVATE|SWP_NOZORDER);SetLayeredWindowAttributes(window,0,220,LWA_ALPHA);auto index=&d-displays.data()+1;identifyLabels[window]=L"Display "+std::to_wstring(index)+L"\n"+d.label+(d.identified?L"":L" (identity unavailable)");identifyWindows.push_back({window,GetTickCount64()+5000});{std::lock_guard lock(registryLock);ownedWindows.insert(window);}
#ifndef DAC_HARNESS
        ShowWindow(window,SW_SHOWNOACTIVATE);
#endif
    }}
void RefreshRuleList(){SendDlgItemMessageW(workspace,320,LB_RESETCONTENT,0,0);for(auto& r:workspaceRules){auto label=std::wstring(r.kind?L"Ignore audio":L"Keep foreground awake")+L" | "+(r.global?L"Global":L"Its attributed displays")+L" | "+(r.nameOnly?L"Name-only: ":L"Full path: ")+Wide(r.executable);SendDlgItemMessageW(workspace,320,LB_ADDSTRING,0,reinterpret_cast<LPARAM>(label.c_str()));}}
bool ApplyWorkspaceRules(){Config c=draft;c.rules=workspaceRules;Config parsed;std::string error;if(!Parse(Serialize(c),parsed,error))return false;draft.rules=parsed.rules;return true;}
void RefreshWorkspaceList(){SendDlgItemMessageW(workspace,300,CB_RESETCONTENT,0,0);for(auto& p:draft.profiles)SendDlgItemMessageW(workspace,300,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(Wide(p.name).c_str()));if(!draft.profiles.empty())SendDlgItemMessageW(workspace,300,CB_SETCURSEL,0,0);}
void WorkspaceSelection(){int index=static_cast<int>(SendDlgItemMessageW(workspace,300,CB_GETCURSEL,0,0));if(index<0||index>=static_cast<int>(draft.profiles.size()))return;auto& p=draft.profiles[index];SetDlgItemTextW(workspace,301,Wide(p.name).c_str());SendDlgItemMessageW(workspace,302,CB_SETCURSEL,p.trigger,0);SetDlgItemTextW(workspace,303,Wide(ClockText(p.start)).c_str());SetDlgItemTextW(workspace,304,Wide(ClockText(p.end)).c_str());SetDlgItemTextW(workspace,305,Wide(p.app).c_str());CheckDlgButton(workspace,306,p.nameOnly?BST_CHECKED:BST_UNCHECKED);}
LRESULT CALLBACK WorkspaceProc(HWND w,UINT msg,WPARAM wp,LPARAM lp){LRESULT result{};if(ThemeMessage(w,msg,wp,lp,result))return result;if(ScrollSettings(w,msg,wp,lp,1240,workspaceScroll,workspaceLayout))return 0;
    if(msg==WM_COMMAND){int id=LOWORD(wp);if(id==300&&HIWORD(wp)==CBN_SELCHANGE)WorkspaceSelection();
        else if(id==310){auto name=Utf8(ControlText(w,301));int start=0,end=0,trigger=static_cast<int>(SendDlgItemMessageW(w,302,CB_GETCURSEL,0,0));if(name.empty()||name.size()>128||!ClockMinute(Utf8(ControlText(w,303)),start)||!ClockMinute(Utf8(ControlText(w,304)),end)||!ApplyWorkspaceRules()){Notice(L"Use a name, valid HH:MM times and valid application rules.");return 0;}
            Profile profile{name,Utf8(ControlText(w,305)),trigger,start,end,IsDlgButtonChecked(w,306)==BST_CHECKED,static_cast<Policy>(draft)};auto found=std::find_if(draft.profiles.begin(),draft.profiles.end(),[&](auto& p){return p.name==name;});if(found==draft.profiles.end()){if(draft.profiles.size()>=8){Notice(L"Eight profiles maximum.");return 0;}draft.profiles.push_back(profile);}else *found=profile;RefreshWorkspaceList();SetDlgItemTextW(w,330,L"Profile captured in unsaved settings draft. Save the main settings to keep it.");}
        else if(id==311||id==312){if(!EditorCurrent())return 0;int index=static_cast<int>(SendDlgItemMessageW(w,300,CB_GETCURSEL,0,0));if(id==311&&(index<0||index>=static_cast<int>(draft.profiles.size())))return 0;draft.manualProfile=id==311?draft.profiles[index].name:"";if(Save(draft,false)){editorBaseline=Serialize(controller.config);SetDlgItemTextW(w,330,L"Profile selection saved. Automatic selection uses app, then schedule, then base; first match wins.");}}
        else if(id==313){int index=static_cast<int>(SendDlgItemMessageW(w,300,CB_GETCURSEL,0,0));if(index>=0&&index<static_cast<int>(draft.profiles.size())){if(draft.manualProfile==draft.profiles[index].name)draft.manualProfile.clear();draft.profiles.erase(draft.profiles.begin()+index);RefreshWorkspaceList();}}
        else if(id==314){int index=static_cast<int>(SendDlgItemMessageW(w,300,CB_GETCURSEL,0,0));if(index<0||index>=static_cast<int>(draft.profiles.size()))return 0;auto path=SelectLocalFile(w,true,L"Export selected profile (hardware grants and automatic activation removed)");DWORD error=0;if(!path.empty()&&!WriteFileText(path,Serialize(Portable(draft.profiles[index].policy)),error))Notice(L"Profile export failed.");}
        else if(id==318){int index=static_cast<int>(SendDlgItemMessageW(w,300,CB_GETCURSEL,0,0));if(index>=0&&index<static_cast<int>(draft.profiles.size())){auto retained=draft.monitors;static_cast<Policy&>(draft)=draft.profiles[index].policy;for(auto& [key,p]:retained)draft.monitors.try_emplace(key,p);DestroyWindow(w);PopulateEditor(false);SetWindowTextW(editor,L"Profile loaded into unsaved draft — review, then capture to replace profile");}return 0;}
        else if(id==323){AppRule rule;rule.kind=static_cast<int>(SendDlgItemMessageW(w,340,CB_GETCURSEL,0,0));rule.global=SendDlgItemMessageW(w,341,CB_GETCURSEL,0,0)==1;rule.nameOnly=SendDlgItemMessageW(w,342,CB_GETCURSEL,0,0)==1;rule.executable=Utf8(ControlText(w,343));Config check=draft;check.rules=workspaceRules;check.rules.push_back(rule);Config parsed;std::string error;if(!Parse(Serialize(check),parsed,error)){Notice(L"Choose an executable path (preferred), or an explicit filename-only match. Maximum 32 rules.");return 0;}workspaceRules=parsed.rules;RefreshRuleList();}
        else if(id==324){int index=static_cast<int>(SendDlgItemMessageW(w,320,LB_GETCURSEL,0,0));if(index>=0&&index<static_cast<int>(workspaceRules.size())){workspaceRules.erase(workspaceRules.begin()+index);RefreshRuleList();}}
        else if(id==325){auto path=SelectLocalFile(w,false,L"Choose application executable");if(!path.empty()){SetDlgItemTextW(w,343,path.c_str());SendDlgItemMessageW(w,342,CB_SETCURSEL,0,0);}}
        else if(id==319&&importPending){int from=static_cast<int>(SendDlgItemMessageW(w,331,CB_GETCURSEL,0,0)),to=static_cast<int>(SendDlgItemMessageW(w,332,CB_GETCURSEL,0,0));if(from>=0&&from<static_cast<int>(importSources.size())&&to>=0&&to<=static_cast<int>(importTargets.size())){auto target=to?importTargets[to-1].id:importSources[from];if(to&&!std::any_of(displays.begin(),displays.end(),[&](auto& d){return d.id==target&&d.identified;})){Notice(L"Selected display changed or disconnected; re-import and choose its stable identity explicitly.");return 0;}if(!to&&std::any_of(displays.begin(),displays.end(),[&](auto& d){return d.id==target;})){Notice(L"This identity is connected: select its local display explicitly.");return 0;}pendingMapping[importSources[from]]=target;SetDlgItemTextW(w,330,(L"Explicit mappings: "+std::to_wstring(pendingMapping.size())+L" / "+std::to_wstring(importSources.size())+L". Map every source, then Apply import.").c_str());}}
        else if(id==315){auto path=SelectLocalFile(w,false,L"Import profile; explicit display mapping required");if(path.empty())return 0;std::string text,error;DWORD code=0;Config parsed;if(!ReadFileText(path,text,code)||!Parse(text,parsed,error)||!parsed.profiles.empty()){Notice(L"Invalid profile file; export a single profile first.");return 0;}pendingImport=parsed;importPending=true;pendingMapping.clear();importSources.clear();SendDlgItemMessageW(w,331,CB_RESETCONTENT,0,0);for(auto& [key,p]:parsed.monitors){(void)p;importSources.push_back(key);SendDlgItemMessageW(w,331,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(Wide(key).c_str()));}SendDlgItemMessageW(w,331,CB_SETCURSEL,0,0);SendDlgItemMessageW(w,332,CB_RESETCONTENT,0,0);SendDlgItemMessageW(w,332,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(L"Retain as disconnected identity"));importTargets=displays;for(auto& d:importTargets)SendDlgItemMessageW(w,332,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(d.label.c_str()));SendDlgItemMessageW(w,332,CB_SETCURSEL,static_cast<WPARAM>(-1),0);SetDlgItemTextW(w,330,L"Choose each imported source identity and its local display, then Map selection. Hardware permission is removed.");}
        else if(id==316&&importPending){Policy imported;auto name=Utf8(ControlText(w,301));if(!MapImported(pendingImport,pendingMapping,imported)||name.empty()||draft.profiles.size()>=8||std::any_of(draft.profiles.begin(),draft.profiles.end(),[&](auto& p){return p.name==name;})){Notice(L"Map every imported display uniquely and choose a new profile name.");return 0;}draft.profiles.push_back({name,"",0,0,0,false,imported});importPending=false;RefreshWorkspaceList();SetDlgItemTextW(w,330,L"Mapped profile added to unsaved draft. Load it into main settings to review and deliberately renew automatic/hardware choices; capture changes back to the profile.");}
        else if(id==321){if(!ApplyWorkspaceRules()){Notice(L"Application rules could not be validated. Choose a full executable path or an explicit filename-only match.");return 0;}DestroyWindow(w);}
        else if(id==322||id==IDCANCEL)DestroyWindow(w);return 0;}
    if(msg==WM_DPICHANGED){SettingsDpiChanged(w,wp,lp,workspaceLayout,workspaceFont,1240,workspaceScroll);return 0;}if(msg==WM_CLOSE){DestroyWindow(w);return 0;}if(msg==WM_DESTROY){workspace=nullptr;importPending=false;if(editor)EnableWindow(editor,TRUE);return 0;}if(msg==WM_NCDESTROY){workspaceLayout.controls.clear();if(workspaceFont)DeleteObject(workspaceFont);workspaceFont=nullptr;}return DefWindowProcW(w,msg,wp,lp);}
void ShowWorkspace(){if(workspace){SetForegroundWindow(workspace);return;}if(!StoreGlobalDraft())return;workspaceLayout={};workspaceScroll=0;workspace=CreateWindowExW(WS_EX_DLGMODALFRAME,kWorkspace,L"Profiles and application rules — settings draft",WS_CAPTION|WS_SYSMENU|WS_VSCROLL,CW_USEDEFAULT,CW_USEDEFAULT,790,800,editor,nullptr,GetModuleHandleW(nullptr),nullptr);if(!workspace)return;FitSettings(workspace,790,800);int dpi=GetDpiForWindow(workspace);workspaceLayout.dpi=dpi;workspaceFont=SettingsFont(dpi);ThemeChrome(workspace);
    auto control=[&](const wchar_t* cls,const wchar_t* text,DWORD style,int x,int y,int width,int height,int id){HWND child=CreateWindowExW(0,cls,text,WS_CHILD|WS_VISIBLE|style,MulDiv(x,dpi,96),MulDiv(y,dpi,96),MulDiv(width,dpi,96),MulDiv(height,dpi,96),workspace,reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),GetModuleHandleW(nullptr),nullptr);workspaceLayout.controls.push_back({child,x,y,width,height});ThemeControl(child,cls);SendMessageW(child,WM_SETFONT,reinterpret_cast<WPARAM>(workspaceFont),TRUE);return child;};
    control(L"STATIC",L"Named profiles capture the main settings draft. Local enable and DDC grants cap all profiles.",0,16,16,725,38,0);control(L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_VSCROLL|WS_TABSTOP,16,62,710,180,300);
    control(L"STATIC",L"Name                                  Automatic trigger",0,16,106,710,22,0);control(L"EDIT",L"Work",WS_BORDER|ES_AUTOHSCROLL|WS_TABSTOP,16,134,300,26,301);control(L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_TABSTOP,335,134,391,180,302);for(auto label:{L"Manual only",L"Local time schedule",L"Foreground application"})SendDlgItemMessageW(workspace,302,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(label));SendDlgItemMessageW(workspace,302,CB_SETCURSEL,0,0);
    control(L"STATIC",L"Schedule start / end in local HH:MM (equal disables; overnight supported)",0,16,175,710,24,0);control(L"EDIT",L"09:00",WS_BORDER|ES_AUTOHSCROLL|WS_TABSTOP,16,204,180,26,303);control(L"EDIT",L"17:00",WS_BORDER|ES_AUTOHSCROLL|WS_TABSTOP,214,204,180,26,304);control(L"BUTTON",L"App match is name-only (less precise)",BS_AUTOCHECKBOX|WS_TABSTOP,414,204,312,26,306);
    control(L"EDIT",L"",WS_BORDER|ES_AUTOHSCROLL|WS_TABSTOP,16,245,710,26,305);control(L"STATIC",L"Foreground app full executable path (preferred), or explicit filename-only match above",0,16,276,710,24,0);
    control(L"BUTTON",L"Capture / replace draft",BS_PUSHBUTTON|WS_TABSTOP,16,312,222,30,310);control(L"BUTTON",L"Save & activate selected",BS_PUSHBUTTON|WS_TABSTOP,254,312,235,30,311);control(L"BUTTON",L"Save & select automatically",BS_PUSHBUTTON|WS_TABSTOP,505,312,221,30,312);
    control(L"BUTTON",L"Delete from draft",BS_PUSHBUTTON|WS_TABSTOP,16,354,222,30,313);control(L"BUTTON",L"Export selected…",BS_PUSHBUTTON|WS_TABSTOP,254,354,235,30,314);control(L"BUTTON",L"Import profile…",BS_PUSHBUTTON|WS_TABSTOP,505,354,221,30,315);
    control(L"BUTTON",L"Load selected policy into main draft",BS_PUSHBUTTON|WS_TABSTOP,16,398,710,30,318);
    control(L"STATIC",L"Application rule: action / scope / preferred full path or explicit name-only match",0,16,448,710,25,0);
    for(int i=0;i<3;++i)control(L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_TABSTOP,16+i*238,480,224,140,340+i);
    for(auto label:{L"Keep foreground awake",L"Ignore this app's audio"})SendDlgItemMessageW(workspace,340,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(label));for(auto label:{L"Its attributed displays",L"Global"})SendDlgItemMessageW(workspace,341,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(label));for(auto label:{L"Full executable path",L"Filename only (less precise)"})SendDlgItemMessageW(workspace,342,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(label));for(int id:{340,341,342})SendDlgItemMessageW(workspace,id,CB_SETCURSEL,0,0);
    control(L"EDIT",L"",WS_BORDER|ES_AUTOHSCROLL|WS_TABSTOP,16,523,480,28,343);control(L"BUTTON",L"Choose executable…",BS_PUSHBUTTON|WS_TABSTOP,510,523,216,28,325);control(L"BUTTON",L"Add rule",BS_PUSHBUTTON|WS_TABSTOP,16,565,345,30,323);control(L"BUTTON",L"Remove selected rule",BS_PUSHBUTTON|WS_TABSTOP,380,565,346,30,324);
    control(L"LISTBOX",L"",WS_BORDER|LBS_NOTIFY|WS_VSCROLL|WS_TABSTOP,16,611,710,122,320);workspaceRules=draft.rules;RefreshRuleList();control(L"STATIC",L"",0,16,747,710,82,330);
    control(L"STATIC",L"Imported source identity / explicit local target",0,16,846,710,25,0);control(L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_VSCROLL|WS_TABSTOP,16,879,710,180,331);control(L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_VSCROLL|WS_TABSTOP,16,924,710,180,332);control(L"BUTTON",L"Map selected source to target",BS_PUSHBUTTON|WS_TABSTOP,16,969,345,30,319);control(L"BUTTON",L"Apply complete import",BS_PUSHBUTTON|WS_TABSTOP,380,969,346,30,316);
    control(L"STATIC",L"Manual selection > foreground app > schedule > base; first listed match wins. Changes debounce for one second. Clock changes use current local time. Import starts with automation and DDC off; load the imported policy into the main draft to review it, then capture it back.",0,16,1024,710,100,0);
    control(L"BUTTON",L"Apply rules to draft / close",BS_PUSHBUTTON|WS_TABSTOP,16,1178,350,32,321);control(L"BUTTON",L"Close",BS_PUSHBUTTON|WS_TABSTOP,388,1178,338,32,322);RefreshWorkspaceList();LayoutSettings(workspace,workspaceLayout,1240,workspaceScroll);EnableWindow(editor,FALSE);
#ifndef DAC_HARNESS
    ShowWindow(workspace,SW_SHOWNORMAL);
#endif
}
// Quick setup retains all display edits until an explicit Save; Save keeps it open.
Config setupDraft;std::string setupBaseline;std::vector<Display> setupDisplays;int setupSelection=-1;
int setupHeight=734;
bool setupLoading=false,setupEdited=false;
HFONT setupHeadingFont{};
void ShowSettings();
void LayoutSetup(HWND w);
void SetupStatus(const wchar_t* text){SetDlgItemTextW(setupWindow,410,text);LayoutSetup(setupWindow);}
void RefreshSetupTypography(){
    LOGFONTW font{};if(!setupFont||!GetObjectW(setupFont,sizeof(font),&font))return;
    font.lfWeight=FW_SEMIBOLD;auto replacement=CreateFontIndirectW(&font);if(!replacement)return;
    for(int id:{420,421})SendDlgItemMessageW(setupWindow,id,WM_SETFONT,reinterpret_cast<WPARAM>(replacement),TRUE);
    if(setupHeadingFont)DeleteObject(setupHeadingFont);setupHeadingFont=replacement;
}
std::wstring SetupDisplayLabel(size_t index){
    const auto& d=setupDisplays[index];auto separator=d.label.find(L" — ");
    auto name=separator==std::wstring::npos?d.label:d.label.substr(separator+3);
    return L"Display "+std::to_wstring(index+1)+L" — "+name;
}
bool StoreSetupDraft(){
    BOOL valid=FALSE;UINT timeout=GetDlgItemInt(setupWindow,404,&valid,FALSE);
    if(!valid||timeout>3600||(timeout>0&&timeout<5)){SetupStatus(L"Idle time must be 5–3600 seconds, or 0 to use the default. Correct this display before saving or switching displays.");return false;}
    setupDraft.automatic=IsDlgButtonChecked(setupWindow,405)==BST_CHECKED;
    setupDraft.perInput=IsDlgButtonChecked(setupWindow,408)==BST_CHECKED;
    if(setupSelection>=0&&setupSelection<static_cast<int>(setupDisplays.size())&&setupDisplays[setupSelection].identified){
        auto& p=setupDraft.monitors[setupDisplays[setupSelection].id];
        p.enabled=IsDlgButtonChecked(setupWindow,402)==BST_CHECKED;
        int choice=static_cast<int>(SendDlgItemMessageW(setupWindow,403,CB_GETCURSEL,0,0));
        if(choice>=0&&choice<3)p.saver=choice==0?-1:choice+7;
        p.timeout=static_cast<int>(timeout);
    }
    return true;
}
void LoadSetupDisplay(){
    setupLoading=true;
    setupSelection=static_cast<int>(SendDlgItemMessageW(setupWindow,401,CB_GETCURSEL,0,0));
    bool selected=setupSelection>=0&&setupSelection<static_cast<int>(setupDisplays.size());
    for(int id:{402,403,404})EnableWindow(GetDlgItem(setupWindow,id),selected&&setupDisplays[setupSelection].identified);
    Preference p;if(selected){auto found=setupDraft.monitors.find(setupDisplays[setupSelection].id);if(found!=setupDraft.monitors.end())p=found->second;}
    CheckDlgButton(setupWindow,402,p.enabled?BST_CHECKED:BST_UNCHECKED);
    SendDlgItemMessageW(setupWindow,403,CB_RESETCONTENT,0,0);
    for(auto label:{L"Native black",L"Moving dim clock",L"Sparse drifting constellation"})SendDlgItemMessageW(setupWindow,403,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(label));
    if(p.saver!=-1&&p.saver!=8&&p.saver!=9)SendDlgItemMessageW(setupWindow,403,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(L"Keep current screensaver / slideshow"));
    SendDlgItemMessageW(setupWindow,403,CB_SETCURSEL,p.saver==-1?0:p.saver==8?1:p.saver==9?2:3,0);
    SetDlgItemInt(setupWindow,404,p.timeout,FALSE);
    setupLoading=false;
}
void LayoutSetup(HWND w){
    RECT client{};GetClientRect(w,&client);int width=std::max(1,MulDiv(client.right,96,setupLayout.dpi)-56);
    auto textHeight=[&](int id,int minimum){
        HDC dc=GetDC(w);if(!dc)return minimum;auto old=setupFont?SelectObject(dc,setupFont):nullptr;
        RECT textRect{0,0,MulDiv(width,setupLayout.dpi,96),0};auto text=ControlText(w,id);
        DrawTextW(dc,text.c_str(),-1,&textRect,DT_CALCRECT|DT_WORDBREAK|DT_NOPREFIX);
        if(old)SelectObject(dc,old);ReleaseDC(w,dc);return std::max(minimum,MulDiv(textRect.bottom,96,setupLayout.dpi)+8);
    };
    int idleHeight=textHeight(425,56),helpHeight=textHeight(426,72),statusHeight=textHeight(410,68),extra=idleHeight-56;
    int statusY=520+extra+helpHeight+12,footerY=statusY+statusHeight+12;setupHeight=footerY+50;
    for(auto& c:setupLayout.controls){
        c.x=28;c.width=width;int id=GetDlgCtrlID(c.window);
        if(id==425)c.height=idleHeight;
        if(id==430)c.y=380+extra;
        if(id==421)c.y=400+extra;
        if(id==405)c.y=438+extra;
        if(id==408)c.y=478+extra;
        if(id==426){c.y=520+extra;c.height=helpHeight;}
        if(id==410){c.y=statusY;c.height=statusHeight;}
        if(id==4||id==406||id==3)c.y=footerY;
        if(id==401)c.width=std::max(1,width-152);
        if(id==407){c.width=140;c.x=28+width-140;}
        if(id==403||id==423)c.width=std::max(1,width-188);
        if(id==404||id==424){c.width=172;c.x=28+width-172;}
        if(id==4)c.width=220;
        if(id==406){c.width=144;c.x=28+width-252;}
        if(id==3){c.width=96;c.x=28+width-96;}
    }
    LayoutSettings(w,setupLayout,setupHeight,setupScroll);
}
void CloseSetup(){
    if(setupEdited){
#ifdef DAC_HARNESS
        int answer=setupCloseResponse;
#else
        int answer=MessageBoxW(setupWindow,L"Discard changes made since your last save? Your saved settings will be kept.",L"Unsaved setup changes",MB_YESNO|MB_ICONQUESTION|MB_DEFBUTTON2);
#endif
        if(answer!=IDYES)return;
    }
    DestroyWindow(setupWindow);
}
LRESULT CALLBACK SetupProc(HWND w,UINT msg,WPARAM wp,LPARAM lp){
    LRESULT result{};if(ThemeMessage(w,msg,wp,lp,result))return result;
    if(msg==DM_GETDEFID)return MAKELRESULT(406,DC_HASDEFID);
    if(msg==WM_GETMINMAXINFO){auto info=reinterpret_cast<MINMAXINFO*>(lp);info->ptMinTrackSize={MulDiv(620,setupLayout.dpi,96),MulDiv(400,setupLayout.dpi,96)};return 0;}
    if(msg==WM_SIZE){LayoutSetup(w);return 0;}
    if(ScrollSettings(w,msg,wp,lp,setupHeight,setupScroll,setupLayout))return 0;
    if(msg==WM_DPICHANGED){SettingsDpiChanged(w,wp,lp,setupLayout,setupFont,setupHeight,setupScroll);LayoutSetup(w);return 0;}
    if(msg==WM_COMMAND){int id=LOWORD(wp),event=HIWORD(wp);
        if(id==407)IdentifyDisplays();
        else if(id==3||id==IDCANCEL)CloseSetup();
        else if(id==401&&event==CBN_SELCHANGE){
            if(StoreSetupDraft()){
                LoadSetupDisplay();
                SetupStatus(setupEdited?L"Unsaved changes are kept while you switch displays. Save setup applies all your display changes.":L"Choose a display to adjust. Save setup keeps this window open.");
            }else SendDlgItemMessageW(w,401,CB_SETCURSEL,setupSelection,0);
        }else if(id==406||id==IDOK||id==4){
            if(!StoreSetupDraft())return 0;
            if(Serialize(controller.config)!=setupBaseline){SetupStatus(L"Saved settings changed elsewhere. Close and reopen setup to load them before saving; your draft has not been applied.");return 0;}
            if(id==4){auto retained=setupDraft;DestroyWindow(w);ShowSettings();if(editor){draft=std::move(retained);PopulateEditor(false);}return 0;}
            if(Save(setupDraft)){
                setupDraft=controller.config;setupBaseline=Serialize(controller.config);setupEdited=false;
                if(editor)PopulateEditor();LoadSetupDisplay();
                SetupStatus(L"Setup saved. You can adjust another display or close this window.");
            }else SetupStatus(saveFailure.c_str());
        }else if(!setupLoading&&((id==404&&event==EN_CHANGE)||(id==403&&event==CBN_SELCHANGE)||((id==402||id==405||id==408)&&event==BN_CLICKED))){
            setupEdited=true;SetupStatus(L"Unsaved changes. Save setup applies display settings and both shared options.");
        }
        return 0;
    }
    if(msg==WM_CLOSE){CloseSetup();return 0;}
    if(msg==WM_DESTROY){if(editor){EnableWindow(editor,TRUE);SetForegroundWindow(editor);}return 0;}
    if(msg==WM_NCDESTROY){setupWindow=nullptr;setupLayout.controls.clear();setupDisplays.clear();setupSelection=-1;if(setupFont)DeleteObject(setupFont);setupFont=nullptr;if(setupHeadingFont)DeleteObject(setupHeadingFont);setupHeadingFont=nullptr;}
    return DefWindowProcW(w,msg,wp,lp);
}
void ShowSetup(){
    if(setupWindow){SetForegroundWindow(setupWindow);return;}
    if(options||workspace){SetForegroundWindow(options?options:workspace);return;}
    if(editor&&!StoreGlobalDraft())return;
    setupDraft=editor?draft:controller.config;setupBaseline=Serialize(controller.config);setupDisplays=displays;
    setupLayout={};setupScroll=0;setupHeight=734;setupLoading=true;setupEdited=false;
    setupWindow=CreateWindowExW(WS_EX_APPWINDOW|WS_EX_CONTROLPARENT,L"DAC-Setup",L"Quick setup — DAC — Display Activity Controls",WS_CAPTION|WS_SYSMENU|WS_THICKFRAME|WS_MAXIMIZEBOX|WS_VSCROLL,CW_USEDEFAULT,CW_USEDEFAULT,760,784,editor,nullptr,GetModuleHandleW(nullptr),nullptr);
    if(!setupWindow){setupLoading=false;return;}
    MONITORINFO monitor{};monitor.cbSize=sizeof(monitor);GetMonitorInfoW(MonitorFromWindow(editor?editor:setupWindow,MONITOR_DEFAULTTONEAREST),&monitor);
    int dpi=GetDpiForWindow(setupWindow);int width=std::min(MulDiv(760,dpi,96),static_cast<int>(monitor.rcWork.right-monitor.rcWork.left)),height=std::min(MulDiv(784,dpi,96),static_cast<int>(monitor.rcWork.bottom-monitor.rcWork.top));
    SetWindowPos(setupWindow,nullptr,monitor.rcWork.left+(monitor.rcWork.right-monitor.rcWork.left-width)/2,monitor.rcWork.top+(monitor.rcWork.bottom-monitor.rcWork.top-height)/2,width,height,SWP_NOZORDER|SWP_NOACTIVATE);
    setupLayout.dpi=GetDpiForWindow(setupWindow);ThemeChrome(setupWindow);setupFont=SettingsFont(setupLayout.dpi,18);
    auto add=[&](const wchar_t* cls,const wchar_t* text,DWORD style,int y,int height,int id){
        HWND child=CreateWindowExW(0,cls,text,WS_CHILD|WS_VISIBLE|style,28,y,680,height,setupWindow,reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),GetModuleHandleW(nullptr),nullptr);
        setupLayout.controls.push_back({child,28,y,680,height});ThemeControl(child,cls);SendMessageW(child,WM_SETFONT,reinterpret_cast<WPARAM>(setupFont?setupFont:GetStockObject(DEFAULT_GUI_FONT)),TRUE);
    };
    add(L"STATIC",L"Choose a display, adjust its protection, then save. Repeat for your other displays without leaving setup.",0,24,52,0);
    add(L"STATIC",L"This &display",0,96,26,420);
    add(L"COMBOBOX",L"",CBS_DROPDOWNLIST|CBS_OWNERDRAWFIXED|CBS_HASSTRINGS|WS_VSCROLL|WS_TABSTOP,132,180,401);
    for(size_t i=0;i<setupDisplays.size();++i){auto label=SetupDisplayLabel(i);SendDlgItemMessageW(setupWindow,401,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(label.c_str()));}
    SendDlgItemMessageW(setupWindow,401,CB_SETCURSEL,setupDisplays.empty()?-1:0,0);
    add(L"BUTTON",L"&Identify (5 s)",BS_PUSHBUTTON|WS_TABSTOP,132,32,407);
    add(L"BUTTON",L"&Enable protection on this display",BS_AUTOCHECKBOX|WS_TABSTOP,180,32,402);
    add(L"STATIC",L"&Protection style",0,232,26,423);
    add(L"COMBOBOX",L"",CBS_DROPDOWNLIST|CBS_OWNERDRAWFIXED|CBS_HASSTRINGS|WS_TABSTOP,264,160,403);
    add(L"STATIC",L"Idle &seconds",0,232,26,424);
    add(L"EDIT",L"",WS_BORDER|ES_NUMBER|WS_TABSTOP,264,32,404);
    auto idleHelp=L"Use 5–3600 seconds, or 0 for the default ("+std::to_wstring(setupDraft.timeout)+L" seconds).\nMore styles and activity options are in Advanced settings.";
    add(L"STATIC",idleHelp.c_str(),0,308,56,425);
    add(L"STATIC",L"",SS_ETCHEDHORZ,380,2,430);
    add(L"STATIC",L"All enabled displays",0,400,26,421);
    add(L"BUTTON",L"Start protection &automatically when idle",BS_AUTOCHECKBOX|WS_TABSTOP,438,32,405);
    CheckDlgButton(setupWindow,405,setupDraft.automatic?BST_CHECKED:BST_UNCHECKED);
    add(L"BUTTON",L"&Independent display input",BS_AUTOCHECKBOX|WS_TABSTOP,478,32,408);
    CheckDlgButton(setupWindow,408,setupDraft.perInput?BST_CHECKED:BST_UNCHECKED);
    std::wstring help=setupDraft.profiles.empty()?L"Independent input: mouse activity wakes its display; keyboard also credits the focused display. Advanced input overrides still apply.":L"This changes base settings. Your profiles can override them; review profiles in Advanced settings.";
    if(editor)help+=L" Save includes the open settings draft.";
    add(L"STATIC",help.c_str(),0,520,72,426);
    add(L"STATIC",L"Choose a display to adjust. Save setup keeps this window open.",0,564,68,410);
    add(L"BUTTON",L"Ad&vanced settings…",BS_PUSHBUTTON|WS_TABSTOP,644,36,4);
    add(L"BUTTON",L"&Save setup",BS_DEFPUSHBUTTON|WS_TABSTOP,644,36,406);
    add(L"BUTTON",L"&Close",BS_PUSHBUTTON|WS_TABSTOP,644,36,3);
    LoadSetupDisplay();RefreshSetupTypography();LayoutSetup(setupWindow);if(editor)EnableWindow(editor,FALSE);
    if(setupDisplays.empty())SetupStatus(L"No displays detected. Advanced settings can refresh the display list.");
#ifndef DAC_HARNESS
    ShowWindow(setupWindow,SW_SHOWNORMAL);SetFocus(GetDlgItem(setupWindow,401));
#endif
}
int editorScroll=0;
LRESULT CALLBACK EditorProc(HWND w,UINT msg,WPARAM wp,LPARAM lp) {
    LRESULT themed{};if(ThemeMessage(w,msg,wp,lp,themed))return themed;
    if(msg==WM_COMMAND) {
        int id=LOWORD(wp);
        if(id==120&&HIWORD(wp)==CBN_SELCHANGE) {
            if(!ValidMonitorNumbers()){SendDlgItemMessageW(editor,120,CB_SETCURSEL,selection,0);MessageBoxW(w,L"Monitor times must be nonnegative integers.",L"Invalid settings",MB_OK);return 0;}
            StoreMonitorDraft(); LoadMonitorDraft();
        }
        else if(id==130) {
            if(!EditorCurrent()){SetWindowTextW(w,L"SETTINGS CHANGED — Reopen before saving | DAC — Display Activity Controls");return 0;}
            if(!ValidMonitorNumbers()){MessageBoxW(w,L"Monitor times must be nonnegative integers.",L"Invalid settings",MB_OK);return 0;}
            StoreMonitorDraft(); BOOL ok1,ok2,ok3;
            draft.timeout=GetDlgItemInt(w,101,&ok1,FALSE); draft.poll=GetDlgItemInt(w,102,&ok2,FALSE); draft.padding=GetDlgItemInt(w,103,&ok3,FALSE);
            if(!ok1||!ok2||!ok3) { MessageBoxW(w,L"Timeout, interval and padding must be nonnegative integers.",L"Invalid settings",MB_OK); return 0; }
            bool* flags[]={&draft.automatic,&draft.perInput,&draft.media,&draft.perMedia,&draft.muted,&draft.debug};
            for(int i=0;i<6;++i) *flags[i]=IsDlgButtonChecked(w,110+i)==BST_CHECKED;
            if(draft.automatic&&LegacyStartupPresent()) { MessageBoxW(w,L"A predecessor DAC controller or old OLED Aegis startup entry was detected. Exit the predecessor and disable the old startup entry before enabling automatic activation here.",L"Startup conflict",MB_OK); return 0; }
            if(Save(draft)) { PopulateEditor(); SetWindowTextW(w,L"Saved — Settings | DAC — Display Activity Controls"); }
            else SetWindowTextW(w,L"SAVE FAILED — Previous settings retained | DAC — Display Activity Controls");
        } else if(id==131) ImportLegacy(w);
        else if(id==132) DestroyWindow(w);
        else if(id==160){if(StoreGlobalDraft()&&selection>=0)StartDraftPreview(draft.monitors[editorDisplays[selection].id]);}
        else if(id==161)ShowWorkspace();else if(id==162)ShowSetup();else if(id==163)IdentifyDisplays();else if(id==164)ExportDiagnostics(w);else if(id==165)MessageBoxW(w,ConflictText().c_str(),L"Read-only conflict diagnostics",MB_OK);
        else if(id==166||id==167){if(MessageBoxW(w,L"Back up saved preferences, then reset the selected monitor or all preferences? Hardware recovery markers remain untouched; unsaved drafts will be replaced.",L"Reset preferences",MB_YESNO|MB_ICONQUESTION)==IDYES){Config next=id==167?Config{}:controller.config;if(id==166&&selection>=0){auto key=editorDisplays[selection].id;next.monitors[key]=Preference{};for(auto& profile:next.profiles)profile.policy.monitors[key]=Preference{};}if(Save(next,true,true))PopulateEditor();}}
        else if(id==168){if(StoreGlobalDraft()){auto retained=draft;auto selected=selection>=0?editorDisplays[selection].id:"";editorDisplays=displays;SendDlgItemMessageW(editor,120,CB_RESETCONTENT,0,0);int selectedIndex=0;for(size_t i=0;i<editorDisplays.size();++i){auto& d=editorDisplays[i];SendDlgItemMessageW(editor,120,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(d.label.c_str()));if(d.id==selected)selectedIndex=static_cast<int>(i);}draft=retained;SendDlgItemMessageW(editor,120,CB_SETCURSEL,selectedIndex,0);LoadMonitorDraft();}}
        else if(id==149){if(ValidMonitorNumbers())ShowSaverOptions();else MessageBoxW(w,L"Monitor times must be nonnegative integers.",L"Invalid settings",MB_OK);}
        else if(id==148&&selection>=0&&selection<static_cast<int>(editorDisplays.size())) {
            auto selected=editorDisplays[selection].id;bool active=false;for(auto& task:powerTasks)if(task->id==selected)active=true;
            if(active)Notice(L"Stop this monitor and wait for hardware cleanup before resetting its fault.");
            else if(MessageBoxW(w,L"Confirm the display is awake using its physical button. Clear the saved hardware fault to permit another explicit power attempt?",L"Reset hardware fault",MB_YESNO|MB_ICONQUESTION)==IDYES) {
                RequestFaultReset(selected);
            }
        }
        return 0;
    }
    if(msg==WM_CLOSE) { DestroyWindow(w); return 0; }
    if(msg==WM_TIMER) {UpdateEditorStatus();return 0;}
    if(ScrollSettings(w,msg,wp,lp,1240,editorScroll,editorLayout))return 0;
    if(msg==WM_DESTROY) {for(auto& r:runs)if(r->contained){r->cancel=true;if(r->window)ShowWindowAsync(r->window,SW_HIDE);}editor=nullptr; selection=-1; return 0; }
    if(msg==WM_NCDESTROY){editorLayout.controls.clear();if(editorFont){DeleteObject(editorFont);editorFont=nullptr;}}
    if(msg==WM_DPICHANGED) {SettingsDpiChanged(w,wp,lp,editorLayout,editorFont,1240,editorScroll);return 0;}
    return DefWindowProcW(w,msg,wp,lp);
}
void ShowSettings() {
    if(setupWindow){SetForegroundWindow(setupWindow);return;}
    if(editor) { SetForegroundWindow(editor); return; }
    UINT dpi=GetDpiForSystem(); int scale=static_cast<int>(dpi);editorLayout={};editorLayout.dpi=scale;
    auto px=[&](int v){return MulDiv(v,scale,96);};
    editorScroll=0;editor=CreateWindowExW(WS_EX_APPWINDOW,kEditor,L"DAC — Display Activity Controls Settings",WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_VSCROLL,
        CW_USEDEFAULT,CW_USEDEFAULT,px(780),px(840),nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    if(!editor) return;
    scale=static_cast<int>(GetDpiForWindow(editor)); if(scale<=0) scale=96;
    editorLayout.dpi=scale;
    FitSettings(editor,780,840);
    editorFont=SettingsFont(scale);ThemeChrome(editor);
    auto control=[&](const wchar_t* cls,const wchar_t* text,DWORD style,int x,int y,int width,int height,int id) {
        HWND c=CreateWindowExW(0,cls,text,WS_CHILD|WS_VISIBLE|style,px(x),px(y),px(width),px(height),editor,reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),GetModuleHandleW(nullptr),nullptr);
        editorLayout.controls.push_back({c,x,y,width,height});ThemeControl(c,cls);
        SendMessageW(c,WM_SETFONT,reinterpret_cast<WPARAM>(editorFont?editorFont:GetStockObject(DEFAULT_GUI_FONT)),TRUE); return c;
    };
    control(L"STATIC",L"Timeout seconds (5–3600) / check interval ms (250–10000) / padding px (0–1024)",0,16,16,655,24,0);
    for(int i=0;i<3;++i) control(L"EDIT",L"",WS_BORDER|WS_TABSTOP|ES_NUMBER,16+i*216,44,195,26,101+i);
    const wchar_t* labels[]={L"Automatic idle activation (requires DAC running)",L"Per-monitor input (keyboard credits cursor and foreground displays)",L"Suppress automatic activation during media playback",L"Per-monitor media attribution (process/window heuristic)",L"Also suppress for muted or silent active audio sessions",L"Optional diagnostics (bounded to 1000 events per host; no titles or input data)"};
    for(int i=0;i<6;++i) control(L"BUTTON",labels[i],BS_AUTOCHECKBOX|WS_TABSTOP,16,84+i*32,650,28,110+i);
    control(L"STATIC",L"Monitor assignment (disconnected preferences are retained)",0,16,284,650,22,0);
    control(L"COMBOBOX",L"",CBS_DROPDOWNLIST|CBS_OWNERDRAWFIXED|CBS_HASSTRINGS|WS_VSCROLL|WS_TABSTOP,16,312,650,160,120);
    control(L"BUTTON",L"Enabled",BS_AUTOCHECKBOX|WS_TABSTOP,16,350,130,26,121);
    control(L"COMBOBOX",L"",CBS_DROPDOWNLIST|CBS_OWNERDRAWFIXED|CBS_HASSTRINGS|WS_VSCROLL|WS_TABSTOP,160,350,506,200,122);
    SendDlgItemMessageW(editor,122,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(L"Native black"));
    for(int i=0;i<6;++i) { auto label=std::wstring(savers[i])+(Available(i)?L" (unqualified)":L" (not installed)"); SendDlgItemMessageW(editor,122,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(label.c_str())); }
    for(auto label:{L"Custom installed .scr",L"Independent photo slideshow",L"Moving dim clock",L"Sparse drifting constellation"})SendDlgItemMessageW(editor,122,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(label));
    control(L"STATIC",L"Monitor idle seconds (0 = global)    Input scope                 Media override",0,16,390,695,22,0);
    control(L"EDIT",L"",WS_BORDER|WS_TABSTOP|ES_NUMBER,16,414,170,26,140);
    control(L"COMBOBOX",L"",CBS_DROPDOWNLIST|CBS_OWNERDRAWFIXED|CBS_HASSTRINGS|WS_VSCROLL|WS_TABSTOP,202,414,225,140,141);
    control(L"COMBOBOX",L"",CBS_DROPDOWNLIST|CBS_OWNERDRAWFIXED|CBS_HASSTRINGS|WS_VSCROLL|WS_TABSTOP,443,414,245,140,142);
    for(auto label:{L"Use global setting",L"Any monitor activity",L"Legacy combined local",L"Pointer display (all real input)",L"Foreground display(s) input"})SendDlgItemMessageW(editor,141,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(label));
    for(auto label:{L"Use global setting",L"Ignore media",L"Suppress for media"})SendDlgItemMessageW(editor,142,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(label));
    control(L"STATIC",L"Seconds after presentation starts:    Switch saver to black (0 = never)    Hardware off (0 = manual only)",0,16,454,700,24,0);
    control(L"EDIT",L"",WS_BORDER|WS_TABSTOP|ES_NUMBER,280,482,170,26,143);
    control(L"EDIT",L"",WS_BORDER|WS_TABSTOP|ES_NUMBER,478,482,210,26,145);
    control(L"BUTTON",L"Opt in: DDC/CI hardware soft-off for this monitor (experimental)",BS_AUTOCHECKBOX|WS_TABSTOP,16,518,690,26,144);
    control(L"BUTTON",L"Keep this monitor awake while a foreground app is fullscreen",BS_AUTOCHECKBOX|WS_TABSTOP,16,550,690,26,146);
    control(L"STATIC",L"",0,16,582,690,44,147);
    control(L"BUTTON",L"Reset hardware fault…",BS_PUSHBUTTON|WS_TABSTOP,16,632,235,30,148);
    control(L"BUTTON",L"Saver files / slideshow / hotkeys…",BS_PUSHBUTTON|WS_TABSTOP,270,632,420,30,149);
    control(L"STATIC",L"Hardware wake can fail: use the monitor's power button. DDC/CI depends on hardware/driver support.\nSave before tray Preview/Configure. Padding can cover adjacent displays.\nActivity/media attribution is heuristic. Sticky mode ignores input; tray Stop and emergency exit remain available.",0,16,674,700,66,0);
    control(L"BUTTON",L"Save",BS_PUSHBUTTON|WS_TABSTOP,16,1194,150,32,130);
    control(L"BUTTON",L"Import legacy INI…",BS_PUSHBUTTON|WS_TABSTOP,182,1194,260,32,131);
    control(L"BUTTON",L"Close",BS_PUSHBUTTON|WS_TABSTOP,458,1194,230,32,132);
    control(L"STATIC",L"Software dim warning: opacity % (0 disables) / fade ms / warning seconds",0,16,760,710,26,0);
    for(int i=0;i<3;++i)control(L"EDIT",L"",WS_BORDER|ES_NUMBER|WS_TABSTOP,16+i*236,792,214,26,150+i);
    control(L"BUTTON",L"On battery, use black instead of expensive content (Unknown keeps assignment)",BS_AUTOCHECKBOX|WS_TABSTOP,16,834,710,26,153);
    control(L"BUTTON",L"Count XInput activity on foreground display(s); unknown attribution credits all",BS_AUTOCHECKBOX|WS_TABSTOP,16,870,710,26,157);
    control(L"STATIC",L"Native scene theme / maximum seconds to black / rotation seconds (0 disables)",0,16,910,710,26,0);control(L"COMBOBOX",L"",CBS_DROPDOWNLIST|CBS_OWNERDRAWFIXED|CBS_HASSTRINGS|WS_TABSTOP,16,942,214,150,154);for(auto label:{L"Dim neutral",L"Dim violet",L"Dim teal"})SendDlgItemMessageW(editor,154,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(label));control(L"EDIT",L"",WS_BORDER|ES_NUMBER|WS_TABSTOP,252,942,214,26,155);control(L"EDIT",L"",WS_BORDER|ES_NUMBER|WS_TABSTOP,488,942,214,26,156);
    const wchar_t* actions[]={L"Preview unsaved draft",L"Profiles / application rules…",L"Quick setup…",L"Identify displays",L"Export diagnostics…",L"Read-only conflicts…",L"Reset selected monitor…",L"Reset all preferences…",L"Refresh display list"};
    for(int i=0;i<9;++i)control(L"BUTTON",actions[i],BS_PUSHBUTTON|WS_TABSTOP,16+(i%3)*236,988+(i/3)*43,220,32,160+i);
    control(L"STATIC",L"Dim is overlay opacity, not hardware brightness. Status refresh retains drafts.\nPreview is contained and unsaved; tray testing uses the saved full-monitor assignment.",0,16,1124,710,54,0);
    PopulateEditor();LayoutSettings(editor,editorLayout,1240,editorScroll);SetTimer(editor,1,500,nullptr);
#ifndef DAC_HARNESS
    ShowWindow(editor,SW_SHOWNORMAL);
#endif
}
void StopMonitor(const std::string& id) {
    auto it=controller.nodes.find(id);if(it!=controller.nodes.end())it->second={GetTickCount64(),++controller.serial,State::Desktop,false};
    for(auto& r:runs)if(r->id==id)r->cancel=true;for(auto& task:powerTasks)if(task->id==id)task->cancel=true;
}
void ManualAll(bool sticky=false) { StopMonitor("@span");for(auto& d:displays) if(d.identified) controller.Manual(d.id,GetTickCount64(),sticky); Reconcile(); }
bool Span(int saver,bool sticky=false,const std::string& source="") {
    if(displays.empty()||controller.blocked||controller.paused)return false;
    for(auto& d:displays)if(!d.identified||!controller.Pref(d.id).enabled)return false;
    Reset();spanningDisplay=displays.front();spanningDisplay.id="@span";spanningDisplay.alias=source.empty()?displays.front().id:source;
    controller.spanPreference=controller.Pref(spanningDisplay.alias);controller.spanPreference.hardware=false;controller.spanPreference.powerAfter=0;
    for(auto& d:displays) {auto& r=spanningDisplay.rect;r.left=std::min(r.left,d.rect.left);r.top=std::min(r.top,d.rect.top);r.right=std::max(r.right,d.rect.right);r.bottom=std::max(r.bottom,d.rect.bottom);}
    controller.nodes["@span"]={};controller.Manual("@span",GetTickCount64(),sticky,saver);Reconcile();return true;
}
void Menu(HWND w) {
    HMENU menu=CreatePopupMenu(); if(!menu) return;
    HMENU presentations=CreatePopupMenu(),automation=CreatePopupMenu(),displayMenu=CreatePopupMenu(),profilesMenu=CreatePopupMenu(),settingsMenu=CreatePopupMenu(),diagnosticsMenu=CreatePopupMenu();
    if(!presentations||!automation||!displayMenu||!profilesMenu||!settingsMenu||!diagnosticsMenu){for(HMENU part:{menu,presentations,automation,displayMenu,profilesMenu,settingsMenu,diagnosticsMenu})if(part)DestroyMenu(part);return;}
    bool menuReady=true;
    auto append=[&](HMENU parent,UINT flags,UINT_PTR id,const wchar_t* label){if(!AppendMenuW(parent,flags,id,label)){menuReady=false;if(flags&MF_POPUP)DestroyMenu(reinterpret_cast<HMENU>(id));}};
    bool stopIntent=controller.Any();std::vector<std::string> menuIds;
    append(menu,MF_STRING,1,stopIntent?L"Stop all":L"Start enabled monitors");
    append(menu,MF_SEPARATOR,0,nullptr);
    append(presentations,MF_STRING,5,L"Start enabled monitors (sticky)");
    append(presentations,MF_STRING,6,L"Span desktop using current monitor's saver");
    append(presentations,MF_STRING,7,L"Black all enabled monitors");
    append(automation,MF_STRING|(controller.paused?MF_CHECKED:0),2,L"Pause for this session");
    append(automation,MF_SEPARATOR,0,nullptr);
    for(int i=0;i<4;++i){int minutes[]={5,15,30,60};auto label=L"Snooze automatic for "+std::to_wstring(minutes[i])+L" minutes";append(automation,MF_STRING,10+i,label.c_str());}
    auto remaining=controller.snoozeUntil>GetTickCount64()?(controller.snoozeUntil-GetTickCount64()+999)/1000:0;auto resume=L"Resume now (snooze "+std::to_wstring(remaining)+L" s remaining)";append(automation,MF_STRING,14,resume.c_str());
    append(displayMenu,MF_STRING,16,L"Identify displays");
    append(displayMenu,MF_SEPARATOR,0,nullptr);
    append(profilesMenu,MF_STRING,29,L"Select profiles automatically");for(size_t i=0;i<controller.config.profiles.size();++i)append(profilesMenu,MF_STRING,30+i,Wide(controller.config.profiles[i].name).c_str());
    append(settingsMenu,MF_STRING,3,L"Settings / import…");
    append(settingsMenu,MF_STRING,15,L"Quick setup…");
    append(settingsMenu,MF_SEPARATOR,0,nullptr);
    append(settingsMenu,MF_STRING|(integration.appearance==0?MF_CHECKED:0),50,L"Appearance: follow Windows");
    append(settingsMenu,MF_STRING|(integration.appearance==1?MF_CHECKED:0),51,L"Appearance: light");
    append(settingsMenu,MF_STRING|(integration.appearance==2?MF_CHECKED:0),52,L"Appearance: dark");
    append(settingsMenu,MF_SEPARATOR,0,nullptr);
    append(settingsMenu,MF_STRING|(integration.trayAction==0?MF_CHECKED:0),53,L"Tray click: start / stop");
    append(settingsMenu,MF_STRING|(integration.trayAction==1?MF_CHECKED:0),54,L"Tray click: Quick setup");
    append(settingsMenu,MF_STRING|(integration.trayAction==2?MF_CHECKED:0),55,L"Tray click: Advanced settings");
    append(settingsMenu,MF_STRING|(integration.startupSetup?MF_CHECKED:0),56,L"Open Quick setup when DAC starts");
    append(settingsMenu,MF_STRING|(LoginStartupState()==1?MF_CHECKED:0),57,L"Start DAC when I sign in");
    append(settingsMenu,MF_SEPARATOR,0,nullptr);
    append(settingsMenu,MF_STRING,58,L"Export all protection settings…");
    append(settingsMenu,MF_STRING,59,L"Import all protection settings…");
    append(settingsMenu,MF_STRING,60,L"About DAC and recovery controls…");
    append(diagnosticsMenu,MF_STRING,18,L"Read-only conflict diagnostics…");
    append(diagnosticsMenu,MF_STRING,17,L"Export redacted diagnostics…");
    for(size_t i=0;i<displays.size();++i) {
        auto& d=displays[i];menuIds.push_back(d.id); HMENU sub=CreatePopupMenu(); if(!sub){menuReady=false;continue;}
        bool enabled=d.identified&&controller.Pref(d.id).enabled;
        append(sub,MF_STRING|(enabled?0:MF_GRAYED),100+i*6,L"Preview saved assignment");
        append(sub,MF_STRING,101+i*6,L"Stop / wake this monitor");
        append(sub,MF_STRING|((d.identified&&controller.Pref(d.id).saver>=0&&controller.Pref(d.id).saver<=6)?0:MF_GRAYED),102+i*6,L"Configure saved saver (shared Windows preferences)");
        append(sub,MF_STRING|(enabled?0:MF_GRAYED),103+i*6,L"Toggle sticky presentation");
        append(sub,MF_STRING|(enabled?0:MF_GRAYED),104+i*6,L"Black this monitor");
        append(sub,MF_STRING|((enabled&&controller.Pref(d.id).hardware)?0:MF_GRAYED),105+i*6,L"Hardware off now (opt-in)");
        auto label=d.label+L" — "+ExplanationText(d.id);
        append(displayMenu,MF_POPUP,reinterpret_cast<UINT_PTR>(sub),label.c_str());
    }
    append(menu,MF_POPUP,reinterpret_cast<UINT_PTR>(presentations),L"Presentations");
    append(menu,MF_POPUP,reinterpret_cast<UINT_PTR>(automation),L"Automation");
    append(menu,MF_POPUP,reinterpret_cast<UINT_PTR>(displayMenu),L"Displays");
    append(menu,MF_POPUP,reinterpret_cast<UINT_PTR>(profilesMenu),L"Profiles");
    append(menu,MF_POPUP,reinterpret_cast<UINT_PTR>(settingsMenu),L"Settings && setup");
    append(menu,MF_POPUP,reinterpret_cast<UINT_PTR>(diagnosticsMenu),L"Diagnostics");
    append(menu,MF_SEPARATOR,0,nullptr);
    append(menu,MF_STRING,4,L"Exit DAC");
    if(!menuReady){DestroyMenu(menu);return;}
    POINT point{}; GetCursorPos(&point); SetForegroundWindow(w);
    UINT choice=TrackPopupMenu(menu,TPM_RETURNCMD|TPM_NONOTIFY,point.x,point.y,0,w,nullptr);
    DestroyMenu(menu); PostMessageW(w,WM_NULL,0,0);
    if(choice==1) { if(stopIntent) Reset(); else ManualAll(); }
    else if(choice==2) { controller.paused=!controller.paused; Reset(); }
    else if(choice==3) ShowSettings(); else if(choice==4) SetEvent(stopEvent);
    else if(choice==5) ManualAll(true);
    else if(choice==6) {HMONITOR monitor=MonitorFromPoint(point,MONITOR_DEFAULTTONEAREST);int saver=-1;std::string source;for(auto& d:displays)if(d.handle==monitor){saver=controller.Pref(d.id).saver;source=d.id;}
        if(!Span(saver,false,source))Notice(L"Spanning requires every connected monitor to be identified and enabled, and protection unpaused.");}
    else if(choice==7) {StopMonitor("@span");for(auto& d:displays)if(d.identified)controller.Manual(d.id,GetTickCount64(),false,-1);Reconcile();}
    else if(choice>=10&&choice<=14){int minutes[]={5,15,30,60,0};controller.Snooze(GetTickCount64(),minutes[choice-10]);Reconcile();}
    else if(choice==15)ShowSetup();else if(choice==16)IdentifyDisplays();else if(choice==17)ExportDiagnostics(w);else if(choice==18)MessageBoxW(w,ConflictText().c_str(),L"Read-only conflicts",MB_OK);
    else if(choice>=50&&choice<=56){auto next=integration;if(choice<=52)next.appearance=static_cast<int>(choice)-50;
        else if(choice<=55)next.trayAction=static_cast<int>(choice)-53;else next.startupSetup=!next.startupSetup;
        DWORD error=0;if(WriteIntegrationSettings(next,error))RefreshSettingsTheme();else Notice((L"Application preference could not be saved (Windows error "+std::to_wstring(error)+L"). Previous settings retained.").c_str());}
    else if(choice==57){DWORD error=0;if(!SetLoginStartup(LoginStartupState()!=1,error))Notice((L"Login startup was not changed. Check profile permissions and remove conflicting DAC startup entries yourself (Windows error "+std::to_wstring(error)+L").").c_str());}
    else if(choice==58)ExportSettings(w);else if(choice==59)ImportSettings(w);
    else if(choice==60)MessageBoxW(w,L"DAC 0.1.0 — Display Activity Controls\nNative Windows standalone development candidate.\n\nEmergency exit: Ctrl+Alt+Shift+F12\nCommand-line recovery: DAC.exe --stop\n\nAutomatic activation and hardware power are opt-in. Hardware power is experimental; physical recovery may be necessary if a monitor/driver does not respond.\n\nAppearance, tray action and optional login startup are in Settings && setup. Windows high contrast takes priority.\n\nMIT License — Copyright 2026 StarlightDaemon\nTheme tokens: Fujin v0.1.0 (MIT).",L"About DAC",MB_OK|MB_ICONINFORMATION);
    else if(choice==29||(choice>=30&&choice<30+controller.config.profiles.size())){Config next=controller.config;next.manualProfile=choice==29?"":next.profiles[choice-30].name;Save(next,false);}
    else if(choice>=100) {
        size_t index=(choice-100)/6; int action=(choice-100)%6; if(index>=menuIds.size()) return;
        auto selected=std::find_if(displays.begin(),displays.end(),[&](const Display& d){return d.id==menuIds[index];});
        if(selected==displays.end()) return;auto& d=*selected;
        StopMonitor("@span");
        if(action==0) controller.Manual(d.id,GetTickCount64());
        else if(action==1) StopMonitor(d.id);
        else if(action==2) { StopMonitor(d.id);Reconcile(); if(!AddRun(d,0,controller.Pref(d.id).saver,true)) Notice(L"Configuration not started; stop existing preview/configuration and retry."); }
        else if(action==3) {if(controller.nodes[d.id].sticky&&Running(controller.nodes[d.id].state))StopMonitor(d.id);else controller.Manual(d.id,GetTickCount64(),true);}
        else if(action==4||action==5) {controller.Manual(d.id,GetTickCount64(),false,-1);
            if(action==5&&Running(controller.nodes[d.id].state))controller.nodes[d.id].powerRequested=true;}
        Reconcile();
    }
}
bool SessionLocked() {
    LPWSTR data=nullptr; DWORD bytes=0;bool result=true;
    if(WTSQuerySessionInformationW(WTS_CURRENT_SERVER_HANDLE,WTS_CURRENT_SESSION,WTSSessionInfoEx,&data,&bytes)) {
        if(bytes>=sizeof(WTSINFOEXW)) {auto info=reinterpret_cast<WTSINFOEXW*>(data);
            result=info->Level!=1||info->Data.WTSInfoExLevel1.SessionState!=WTSActive||info->Data.WTSInfoExLevel1.SessionFlags!=WTS_SESSIONSTATE_UNLOCK;}
        WTSFreeMemory(data);
    } return result;
}
LRESULT CALLBACK WindowProc(HWND w,UINT msg,WPARAM wp,LPARAM lp) {
    if(taskbar&&msg==taskbar&&w==ui) { ++trayEpoch;trayOutstanding=false;requestedTray=-1;trayPresent=false; SetBlocked(ResetCause::Tray);nextTray=0; return 0; }
    switch(msg) {
#ifdef DAC_HARNESS
    case WM_APP+100: {ShowSettings();bool created=editor&&GetDlgItem(editor,130)&&GetDlgItem(editor,122);if(editor) DestroyWindow(editor);return created&&editorFont==nullptr;}
    case WM_APP+101: return reinterpret_cast<LRESULT(*)()>(lp)(); // same-process test action on actual UI owner
#endif
    case WM_MOUSEACTIVATE:for(auto& r:runs)if(r->window==w&&r->contained)return MA_ACTIVATE;return MA_NOACTIVATE;
    case WM_SETCURSOR:for(auto& r:runs)if(r->window==w&&(r->contained||r->presentation==-3))return DefWindowProcW(w,msg,wp,lp);if(w!=ui){SetCursor(nullptr);return TRUE;}break;
    case WM_PAINT:{Measure paintMeasure{wakeTiming.paintMaxUs,&wakeTiming.paintUs};Count(wakeTiming.paintSamples);PAINTSTRUCT p{}; HDC dc=BeginPaint(w,&p); FillRect(dc,&p.rcPaint,static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));
        if(identifyLabels.contains(w)){RECT bounds{};GetClientRect(w,&bounds);SetTextColor(dc,RGB(210,210,210));SetBkMode(dc,TRANSPARENT);auto font=SettingsFont(GetDpiForWindow(w));auto previous=font?SelectObject(dc,font):nullptr;DrawTextW(dc,identifyLabels[w].c_str(),-1,&bounds,DT_CENTER|DT_VCENTER|DT_WORDBREAK|DT_NOPREFIX);if(previous)SelectObject(dc,previous);if(font)DeleteObject(font);}
        for(auto& r:runs)if(r->window==w){RECT bounds{};GetClientRect(w,&bounds);
            Maximum(wakeTiming.paintPixelsMax,static_cast<Time>(std::max(0L,bounds.right))*static_cast<Time>(std::max(0L,bounds.bottom)));
            if(r->presentation<0)Count(wakeTiming.blackPaints);else if(r->presentation==8)Count(wakeTiming.clockPaints);else if(r->presentation==9)Count(wakeTiming.constellationPaints);
            if(r->presentation>=8)PaintScene(dc,bounds,*r,GetTickCount64());else if(r->presentation==7){std::lock_guard lock(r->frameLock);if(r->frame){Gdiplus::Graphics graphics(dc);int offset=r->padding;graphics.DrawImage(r->frame.get(),offset,offset,bounds.right-2*offset,bounds.bottom-2*offset);}}break;}
        EndPaint(w,&p); return 0;}
    case WM_HOTKEY:
        if(wp==11){if(controller.Any())Reset();else ManualAll(true);}
        else if(wp==10){POINT point{};if(GetCursorPos(&point)){HMONITOR monitor=MonitorFromPoint(point,MONITOR_DEFAULTTONULL);StopMonitor("@span");for(auto& d:displays)if(d.handle==monitor&&d.identified){if(Running(controller.nodes[d.id].state))StopMonitor(d.id);else controller.Manual(d.id,GetTickCount64(),true);}}}Reconcile();return 0;
    case WM_INPUT:Count(wakeTiming.receipts);Input(reinterpret_cast<HRAWINPUT>(lp));break;
    case kTray:if(lp==WM_RBUTTONUP) Menu(w); else if(lp==WM_LBUTTONUP) {
        if(integration.trayAction==1)ShowSetup();else if(integration.trayAction==2)ShowSettings();
        else if(controller.Any()) Reset();else ManualAll();
    } return 0;
    case kReload:Reload();return 0;
    case kIntegration:ReadIntegrationSettings();RefreshSettingsTheme();return 0;
    case WM_SETTINGCHANGE:case WM_THEMECHANGED:case WM_SYSCOLORCHANGE:RefreshSettingsTheme();return 0;
    case WM_DISPLAYCHANGE:case WM_DPICHANGED:if(w==ui){Reset(ResetCause::Topology);topologyPending=true;}else if(msg==WM_DPICHANGED&&lp){auto r=reinterpret_cast<RECT*>(lp);SetWindowPos(w,nullptr,r->left,r->top,r->right-r->left,r->bottom-r->top,SWP_NOZORDER|SWP_NOACTIVATE);}return 0;
    case WM_WTSSESSION_CHANGE:
        if(wp==WTS_SESSION_LOCK||wp==WTS_CONSOLE_DISCONNECT||wp==WTS_REMOTE_DISCONNECT) locked=true;
        if(wp==WTS_SESSION_UNLOCK||wp==WTS_CONSOLE_CONNECT||wp==WTS_REMOTE_CONNECT) {locked=SessionLocked();topologyPending=true;}
        SetBlocked(ResetCause::Session);return 0;
    case WM_POWERBROADCAST:{bool wasBlocked=locked||suspended||displayOff||!trayPresent;
        if(wp==PBT_APMSUSPEND) suspended=true;
        if(wp==PBT_APMRESUMEAUTOMATIC||wp==PBT_APMRESUMESUSPEND) {suspended=false;topologyPending=true;}
        if(wp==PBT_POWERSETTINGCHANGE) { auto p=reinterpret_cast<POWERBROADCAST_SETTING*>(lp);
            if(p&&p->PowerSetting==GUID_CONSOLE_DISPLAY_STATE&&p->DataLength==sizeof(DWORD)) { DWORD state=0; memcpy(&state,p->Data,sizeof(state)); displayOff=state==0; }
        } if(wasBlocked!=(locked||suspended||displayOff||!trayPresent)||wp==PBT_APMRESUMEAUTOMATIC||wp==PBT_APMRESUMESUSPEND)SetBlocked(ResetCause::Power);return TRUE;}
    case WM_TIMER:{
        Measure timerMeasure{wakeTiming.timerMaxUs,&wakeTiming.timerUs};Count(wakeTiming.heartbeatSamples);auto heartbeat=GetTickCount64();
        if(heartbeatAt){Maximum(wakeTiming.heartbeatMaxMs,heartbeat-heartbeatAt);Sample(wakeTiming.heartbeatMs,heartbeat-heartbeatAt);}heartbeatAt=heartbeat;
        if(WaitForSingleObject(stopEvent,0)==WAIT_OBJECT_0) { PostMessageW(w,kQuit,0,0); return 0; }
        if(topologyPending) { RefreshTopology(); }
        auto now=GetTickCount64();
        for(auto it=identifyWindows.begin();it!=identifyWindows.end();){if(now>=it->second){{std::lock_guard lock(registryLock);ownedWindows.erase(it->first);}identifyLabels.erase(it->first);DestroyWindow(it->first);it=identifyWindows.erase(it);}else ++it;}
#ifdef DAC_HARNESS
        if(!injectedObservations) {CheckInputFallback();ObserveForeground();}
#else
        CheckInputFallback();ObserveForeground();
#endif
#ifdef DAC_HARNESS
        if(!injectedObservations){
#endif
        AdapterSnapshot sample;{std::lock_guard lock(observationLock);sample=adapters;}
        ConsumeAdapters(sample,now);
#ifdef DAC_HARNESS
        }
#endif
        SYSTEMTIME local{};GetLocalTime(&local);if(controller.SelectPolicy(now,local.wHour*60+local.wMinute)){InvalidateMedia();nextPoll=0;}
        if(now>=nextPoll) { Media m; {std::lock_guard lock(observationLock);m=observation;} controller.Tick(now,m);nextPoll=now+std::min(controller.PolicyNow().poll,250); }
        // Identity failures and clone sources are ineligible even for automatic policy.
        for(auto& d:displays) if(!d.identified) controller.nodes[d.id].state=State::Disabled;
        controller.hardwareFaults.clear();for(size_t i=0;i<displays.size();++i){auto& d=displays[i];bool activePower=false;for(auto& task:powerTasks)if(task->id==d.id&&!task->done&&task->status<=1)activePower=true;if(CachedFault(d.id)&&!activePower)controller.hardwareFaults.insert(d.id);Media reasonObservation;{std::lock_guard lock(observationLock);reasonObservation=observation;}auto e=controller.Explain(d.id,reasonObservation,now);if(!lastReasons.contains(d.id)||lastReasons[d.id]!=e.primary){lastReasons[d.id]=e.primary;Record("Display "+std::to_string(i+1),ReasonText(e.primary));}}
        Reconcile();
        Maintain(now);return 0;
    }
    case WM_QUERYENDSESSION:SetEvent(stopEvent);return TRUE;
    case WM_CLOSE:if(w==ui)SetEvent(stopEvent);else for(auto& r:runs)if(r->window==w&&r->contained){r->cancel=true;ShowWindow(w,SW_HIDE);}return 0;
    case WM_SIZE:for(auto& r:runs)if(r->window==w&&r->contained&&r->preview)SetWindowPos(r->preview,nullptr,0,0,LOWORD(lp),HIWORD(lp),SWP_NOZORDER|SWP_NOACTIVATE);break;
    case kQuit:EndMenu();PostQuitMessage(0);return 0;
    }
    return DefWindowProcW(w,msg,wp,lp);
}
DWORD WINAPI SafetyMain(void*) {
    MSG msg{}; PeekMessageW(&msg,nullptr,0,0,PM_NOREMOVE);
#ifdef DAC_HARNESS
    safetyOK=true; // event-driven cancellation tested; never steals the operator desktop's hotkey
#else
    safetyOK=!!RegisterHotKey(nullptr,1,MOD_CONTROL|MOD_ALT|MOD_SHIFT|MOD_NOREPEAT,VK_F12);
#endif
    SetEvent(safetyReady);
    if(!safetyOK) SetEvent(stopEvent);
    while(safetyOK) {
        DWORD status=MsgWaitForMultipleObjects(1,&stopEvent,FALSE,250,QS_ALLINPUT);
        if(status==WAIT_OBJECT_0||status==WAIT_FAILED) break;
        while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)) if(msg.message==WM_HOTKEY) SetEvent(stopEvent);
    }
    BeginShutdownDeadline();
    { std::lock_guard lock(registryLock); for(auto window:ownedWindows) ShowWindowAsync(window,SW_HIDE); for(auto& [job,window]:ownedJobs) {
        if(window) ShowWindowAsync(window,SW_HIDE); TerminateJobObject(job,0);
    } }
    PostMessageW(ui,kQuit,0,0); UnregisterHotKey(nullptr,1); return 0;
}
DWORD WINAPI UiMain(void*) {
    bool dpiOK=SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2)!=nullptr;
    HRESULT com=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
    Gdiplus::GdiplusStartupInput imagingInput;bool imagingOK=Gdiplus::GdiplusStartup(&imagingToken,&imagingInput,nullptr)==Gdiplus::Ok;
#ifdef DAC_HARNESS
    // Harness never reads/writes the operator's saved preferences or arms idle.
    wchar_t testPath[32768]; GetModuleFileNameW(nullptr,testPath,32768); configPath=std::wstring(testPath)+L".test-settings";
#else
    configPath=ConfigPath();
#endif
    ReadIntegrationSettings();
    ReadUiTheme();if(imagingOK){theme.smallIcon=DacIcon(GetSystemMetrics(SM_CXSMICON),false);theme.largeIcon=DacIcon(GetSystemMetrics(SM_CXICON),false);theme.activeIcon=DacIcon(GetSystemMetrics(SM_CXSMICON),true);}
    WNDCLASSW wc{}; wc.lpfnWndProc=WindowProc; wc.hInstance=GetModuleHandleW(nullptr); wc.lpszClassName=kClass;
    wc.hCursor=LoadCursorW(nullptr,IDC_ARROW); wc.hbrBackground=static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
    bool registered=!!RegisterClassW(&wc); wc.lpfnWndProc=EditorProc; wc.lpszClassName=kEditor; wc.hbrBackground=reinterpret_cast<HBRUSH>(COLOR_WINDOW+1);
    bool editorRegistered=registered&&RegisterClassW(&wc);
    wc.lpfnWndProc=OptionsProc;wc.lpszClassName=kOptions;bool optionsRegistered=editorRegistered&&RegisterClassW(&wc);
    wc.lpfnWndProc=WorkspaceProc;wc.lpszClassName=kWorkspace;bool workspaceRegistered=optionsRegistered&&RegisterClassW(&wc);wc.lpfnWndProc=SetupProc;wc.lpszClassName=L"DAC-Setup";bool setupRegistered=workspaceRegistered&&RegisterClassW(&wc);
    if(setupRegistered) ui=CreateWindowExW(WS_EX_TOOLWINDOW,kClass,L"DAC — Display Activity Controls controller",WS_POPUP,0,0,0,0,nullptr,nullptr,wc.hInstance,nullptr);
    RAWINPUTDEVICE raw[]={{1,2,RIDEV_INPUTSINK,ui},{1,6,RIDEV_INPUTSINK,ui}};
    bool inputOK=ui&&RegisterRawInputDevices(raw,2,sizeof(raw[0]));
    taskbar=RegisterWindowMessageW(L"TaskbarCreated");
    bool sessionOK=ui&&WTSRegisterSessionNotification(ui,NOTIFY_FOR_THIS_SESSION);
    locked=SessionLocked();
    controller.blocked=locked;
    if(ui) powerNotification=RegisterPowerSettingNotification(ui,&GUID_CONSOLE_DISPLAY_STATE,DEVICE_NOTIFY_WINDOW_HANDLE);
    std::wstring startupNotice;
    std::string text,error; DWORD code=0;
    if(!configPath.empty()&&ReadFileText(configPath,text,code)) {
        if(!Parse(text,controller.config,error)) { controller.config.automatic=false; startupNotice=L"Invalid configuration: automatic activation disabled. Use backed-up Reset all preferences or preserve and repair the original file."; }
    } else if(code!=ERROR_FILE_NOT_FOUND&&code!=ERROR_PATH_NOT_FOUND) {controller.config.automatic=false;startupNotice=L"Cannot read configuration. Automatic activation disabled; check settings storage.";}
    controller.legacyAutomationBlocked=LegacyStartupPresent();
    if(controller.legacyAutomationBlocked) { controller.config.automatic=false; startupNotice=L"A predecessor controller or old startup entry was detected; automatic activation is withheld. Exit the predecessor and disable the old startup entry before enabling Automatic."; }
    debug=controller.config.debug; RefreshTopology();
    bool maintenanceOK=false;
    if(ui){try{StartMaintenance();maintenanceOK=true;Maintain(GetTickCount64());}catch(...){maintenanceOK=false;}}
    // Startup precedes input/session activation. Let a quick initial tray result
    // establish availability; a slow shell remains blocked and recovers later.
    if(maintenanceOK){Time until=GetTickCount64()+100;while(!trayPresent&&GetTickCount64()<until){Sleep(1);Maintain(GetTickCount64());}}
#ifdef DAC_HARNESS
    controller.config.automatic=false; // test exemptions, never in production
#endif
    controller.blocked=locked||!trayPresent;
    ready=maintenanceOK&&dpiOK&&imagingOK&&theme.smallIcon&&theme.largeIcon&&theme.activeIcon&&ui&&inputOK&&sessionOK&&powerNotification&&SetTimer(ui,1,50,nullptr)&&!configPath.empty();
    if(ready)RegisterControls();
    if(ready&&!startupNotice.empty()) Notice(startupNotice.c_str());
#ifndef DAC_HARNESS
    if(ready&&(text.empty()||(!backgroundStart&&OpenSetupOnStart(false))))ShowSetup();
#endif
    SetEvent(readyEvent); MSG msg{};
    if(ready) while(true) { int status=GetMessageW(&msg,nullptr,0,0); if(status<=0) break;
        dispatchedMessage=msg;HWND dialog=setupWindow?setupWindow:workspace?workspace:options?options:editor;if(!dialog||!IsDialogMessageW(dialog,&msg)) { TranslateMessage(&msg); DispatchMessageW(&msg); }dispatchedMessage={};
    }
    BeginShutdownDeadline();SetEvent(stopEvent); StopRuns();StopMaintenance();
    for(auto& task:powerTasks)if(task->worker.joinable())task->worker.join();powerTasks.clear();
    for(auto& r:runs) { if(r->worker.joinable()) r->worker.join(); if(r->window) { {std::lock_guard lock(registryLock);ownedWindows.erase(r->window);} DestroyWindow(r->window); } } runs.clear();
    if(workspace)DestroyWindow(workspace);if(setupWindow)DestroyWindow(setupWindow);for(auto& item:identifyWindows){{std::lock_guard lock(registryLock);ownedWindows.erase(item.first);}DestroyWindow(item.first);}identifyWindows.clear();identifyLabels.clear();
    if(editor) DestroyWindow(editor);
    RAWINPUTDEVICE remove[]={{1,2,RIDEV_REMOVE,nullptr},{1,6,RIDEV_REMOVE,nullptr}}; if(inputOK) RegisterRawInputDevices(remove,2,sizeof(remove[0]));
    if(powerNotification) { UnregisterPowerSettingNotification(powerNotification); powerNotification=nullptr; }
    if(ui) { UnregisterHotKey(ui,10);UnregisterHotKey(ui,11);KillTimer(ui,1); WTSUnRegisterSessionNotification(ui); DestroyWindow(ui); ui=nullptr; }
    if(setupRegistered)UnregisterClassW(L"DAC-Setup",wc.hInstance);if(workspaceRegistered)UnregisterClassW(kWorkspace,wc.hInstance);
    if(optionsRegistered)UnregisterClassW(kOptions,wc.hInstance);
    if(editorRegistered) UnregisterClassW(kEditor,wc.hInstance); if(registered) UnregisterClassW(kClass,wc.hInstance);
    if(SUCCEEDED(com)) CoUninitialize();
    for(auto icon:{theme.smallIcon,theme.largeIcon,theme.activeIcon})if(icon)DestroyIcon(icon);theme.smallIcon=theme.largeIcon=theme.activeIcon=nullptr;
    if(theme.base)DeleteObject(theme.base);if(theme.surface)DeleteObject(theme.surface);theme.base=theme.surface=nullptr;
    if(imagingOK){Gdiplus::GdiplusShutdown(imagingToken);imagingToken=0;}

    return 0;
}
void Shutdown() {
    BeginShutdownDeadline();unloading=true; if(stopEvent) SetEvent(stopEvent); if(ui) PostMessageW(ui,kQuit,0,0);
    // Graceful joins retain ownership. The standalone supervisor independently
    // enforces an eight-second process deadline if an OS call does not return.
    for(HANDLE h:{safetyThread,mediaThread,uiThread}) if(h) WaitForSingleObject(h,INFINITE);
    // Natural UI exit may also wait on worker handles. Close only after ALL
    // threads have ended, so no waiter can observe a reused/closed handle.
    for(HANDLE* h:{&safetyThread,&mediaThread,&uiThread}) if(*h) {CloseHandle(*h);*h=nullptr;}
    for(HANDLE* h:{&stopEvent,&readyEvent,&safetyReady,&singleton}) if(*h) { CloseHandle(*h); *h=nullptr; }
    ready=false;
}
bool Initialize() {
    unloading=false; ready=false; safetyOK=false; startupComplete=false; controller=Controller{};
    locked=suspended=displayOff=topologyPending=false; lastInputStamp=rawStamp=0; diagnostics=0;
    shutdownRequestedAt=0;heartbeatAt=0;nextPoll=nextTray=0;trayPresent=false;powerStatus.clear();adapters={};consumedControllerInput=0;lastReasons.clear();
#ifdef DAC_HARNESS
    singleton=CreateMutexW(nullptr,FALSE,(testStopName+L"-singleton").c_str());
#else
    if(objectScope.empty())return false;
    singleton=CreateMutexW(nullptr,FALSE,(objectScope+L"-Singleton").c_str());
#endif
    if(!singleton||GetLastError()==ERROR_ALREADY_EXISTS) { if(singleton) CloseHandle(singleton); singleton=nullptr;return false; }

    stopEvent=CreateEventW(nullptr,TRUE,FALSE,kStopName); readyEvent=CreateEventW(nullptr,TRUE,FALSE,nullptr); safetyReady=CreateEventW(nullptr,TRUE,FALSE,nullptr);
    if(!stopEvent||!readyEvent||!safetyReady) { Shutdown(); return false; }
#ifdef DAC_HARNESS
    if(initFault==1) {Shutdown();return false;}
#endif
    safetyThread=CreateThread(nullptr,0,SafetyMain,nullptr,0,nullptr);
    if(!safetyThread||WaitForSingleObject(safetyReady,5000)!=WAIT_OBJECT_0||!safetyOK) { Shutdown(); return false; }
#ifdef DAC_HARNESS
    if(initFault==2) {Shutdown();return false;}
#endif
    uiThread=CreateThread(nullptr,0,UiMain,nullptr,0,nullptr);
    if(!uiThread||WaitForSingleObject(readyEvent,10000)!=WAIT_OBJECT_0||!ready) { Shutdown(); return false; }
#ifdef DAC_HARNESS
    if(initFault==3) {Shutdown();return false;}
#endif
    mediaThread=CreateThread(nullptr,0,MediaMain,nullptr,0,nullptr);
    if(!mediaThread) { Shutdown(); return false; }
    startupComplete=true;
    return true;
}
} // namespace dac
