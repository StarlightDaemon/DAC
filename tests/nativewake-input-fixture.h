#pragma once
#include <windows.h>
#include <atomic>
// Isolated test input adapter. No SendInput, actual pointer movement, or live
// keyboard events. Used by both current integration and old-source control.
namespace wake_fixture {
inline std::atomic<bool> enabled{},readFailure{},overrideQueue{};
inline std::atomic<unsigned> rawReads{},foregroundQueries{};
inline POINT currentPoint{1200,100};inline HMONITOR currentMonitor=reinterpret_cast<HMONITOR>(3);
inline RAWINPUT packet{};inline MSG message{};inline HWND focus{};
inline UINT WINAPI Raw(HRAWINPUT h,UINT command,LPVOID data,PUINT size,UINT header){if(!enabled)return GetRawInputData(h,command,data,size,header);++rawReads;if(readFailure){SetLastError(ERROR_INVALID_HANDLE);return UINT(-1);}*static_cast<RAWINPUT*>(data)=packet;*size=sizeof(packet);return sizeof(packet);}
inline LONG WINAPI Stamp(){return enabled?static_cast<LONG>(message.time):GetMessageTime();}
inline DWORD WINAPI Position(){return enabled?MAKELONG(static_cast<short>(message.pt.x),static_cast<short>(message.pt.y)):GetMessagePos();}
inline BOOL WINAPI Cursor(LPPOINT point){if(!enabled)return GetCursorPos(point);*point=currentPoint;return TRUE;}
inline HMONITOR WINAPI Monitor(POINT point,DWORD flags){return enabled?currentMonitor:MonitorFromPoint(point,flags);}
inline HWND WINAPI Foreground(){if(enabled){++foregroundQueries;return focus;}return GetForegroundWindow();}
inline BOOL WINAPI Visible(HWND window){return enabled&&focus&&window==focus?TRUE:IsWindowVisible(window);}
inline BOOL WINAPI Message(LPMSG msg,HWND window,UINT first,UINT last){BOOL result=GetMessageW(msg,window,first,last);if(result>0&&enabled&&overrideQueue&&msg->message==WM_INPUT){msg->pt=message.pt;msg->time=message.time;}return result;}
}
#define GetRawInputData wake_fixture::Raw
#define GetMessageTime wake_fixture::Stamp
#define GetMessagePos wake_fixture::Position
#define GetCursorPos wake_fixture::Cursor
#define MonitorFromPoint wake_fixture::Monitor
#define GetForegroundWindow wake_fixture::Foreground
#define IsWindowVisible wake_fixture::Visible
#define GetMessageW wake_fixture::Message
