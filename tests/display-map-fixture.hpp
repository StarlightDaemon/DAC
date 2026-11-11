// SPDX-License-Identifier: MIT
// Include after features-platform.hpp in the initialized private native runner.
#pragma once
LRESULT FeatureDisplayMapGeometry() {
    std::vector<RECT> desktop{{-1080,-240,0,1680},{0,0,3840,2160},{4200,300,6120,1380}};
    RECT viewport{10,10,1010,510};auto fitted=MapFit(desktop,viewport);
    Check(fitted.size()==3&&fitted[0].left<fitted[1].left&&fitted[0].top<fitted[1].top,"map signed negative and portrait desktop placement survives fitting");
    Check(fitted[0].bottom-fitted[0].top>fitted[0].right-fitted[0].left&&fitted[2].left>fitted[1].right,"map preserves portrait aspect and physical desktop gaps");
    // The same desktop pixel rectangles fit identically regardless of an
    // individual display's DPI: no monitor DPI enters this transform.
    auto mixedDpi=MapFit(desktop,viewport);Check(EqualRect(&fitted[1],&mixedDpi[1]),"map fitting operates on desktop pixels without per-monitor DPI distortion");
    std::vector<RECT> extreme{{(-2147483647L-1),(-2147483647L-1),-1,0},{0,0,2147483647L,2147483647L}};
    auto large=MapFit(extreme,viewport);for(auto r:large)Check(MapValidRect(r)&&r.left>=viewport.left&&r.right<=viewport.right&&r.top>=viewport.top&&r.bottom<=viewport.bottom,"map full signed LONG desktop extents fit without overflow");
    auto empty=MapFit({{0,0,0,1}},viewport);Check(!MapValidRect(empty[0])&&MapFit(desktop,{0,0,0,0}).size()==desktop.size(),"map invalid bounds and empty viewport safely produce no hit targets");
    Check(MapOverlap(desktop[1],desktop[1])&&!MapOverlap({0,0,100,100},{100,0,200,100})&&MapOverlap({0,0,100,100},{99,50,200,200}),"map clone and overlap classification excludes edge adjacency");
    return 1;
}
void FeatureMapSelect(const std::string& id) {
    auto found=std::find(mapIds.begin(),mapIds.end(),id);Check(found!=mapIds.end(),"map fixture stable target exists");
    SendDlgItemMessageW(displayMap,MapList,LB_SETCURSEL,static_cast<int>(found-mapIds.begin()),0);
    SendMessageW(displayMap,WM_COMMAND,MAKEWPARAM(MapList,LBN_SELCHANGE),reinterpret_cast<LPARAM>(GetDlgItem(displayMap,MapList)));
}
LRESULT FeatureDisplayMapNative() {
    SetupSessions();topologyPending=false;mapSelected.clear();
    // Set deterministic desktop geometry; no OS enumeration or live GUI.
    displays[0].rect={-1080,0,0,1920};displays[1].rect={0,0,3840,2160};displays[2].rect={4200,0,6120,1080};
    auto config=Serialize(controller.config);auto peer=controller.nodes.at("A"),selected=controller.nodes.at("B");
    ShowDisplayMap();Check(displayMap&&mapFont&&!IsWindowVisible(displayMap),"map creates hidden native harness surface");
    {std::lock_guard lock(registryLock);Check(ownedWindows.contains(displayMap),"map registers owned foreground surface");}
    auto window=displayMap,list=GetDlgItem(displayMap,MapList);FeatureMapSelect("B");SetFocus(list);auto focus=GetFocus();
    std::reverse(displays.begin(),displays.end());++topologyEpoch;MapRefresh();
    int index=static_cast<int>(SendMessageW(list,LB_GETCURSEL,0,0));Check(index>=0&&mapIds[index]=="B"&&mapSelected=="B"&&GetFocus()==focus,"map selection and focus follow identity through topology reordering");
    auto length=static_cast<int>(SendMessageW(list,LB_GETTEXTLEN,index,0));std::wstring native(static_cast<size_t>(length)+1,L'\0');SendMessageW(list,LB_GETTEXT,index,reinterpret_cast<LPARAM>(native.data()));native.resize(length);
    Check(native.find(L"Enabled")!=std::wstring::npos&&native.find(L"desktop pixels")!=std::wstring::npos&&native.find(L"Injected B")!=std::wstring::npos,"map native accessible list provides complete name, enabled state and signed geometry");
    Check(Serialize(controller.config)==config&&FeaturePlatformNodeSame(peer,controller.nodes.at("A"))&&FeaturePlatformNodeSame(selected,controller.nodes.at("B")),"map open/select/reorder never changes policy or display controller state");
    ++topologyEpoch;auto previousFlyoutSelection=flyoutSelected;
    SendMessageW(displayMap,WM_COMMAND,MAKEWPARAM(MapControls,BN_CLICKED),0);Check(flyoutSelected==previousFlyoutSelection&&ControlText(displayMap,MapDetail).find(L"snapshot changed")!=std::wstring::npos,"map stale route rejects target before refresh");
    MapRefresh();topologyPending=true;MapRefresh();Check(!IsWindowEnabled(GetDlgItem(displayMap,MapControls))&&!MapTarget(false),"map pending discovery disables target routes");topologyPending=false;
    for(auto& d:displays)if(d.id=="A")for(auto& b:displays)if(b.id=="B")d.rect=b.rect;
    MapRefresh();Check(!MapTarget(true)&&MapTarget(false)&&ControlText(displayMap,MapDetail).find(L"clone or overlapping")!=std::wstring::npos,"map overlapping clone targets are accessible and ineligible for controls");
    for(auto& d:displays)if(d.id=="B")d.identified=false;MapRefresh();Check(ControlText(displayMap,MapDetail).find(L"unknown target")!=std::wstring::npos&&!MapTarget(true),"map unknown target status explicitly explains unavailable controls");
    displays.erase(std::remove_if(displays.begin(),displays.end(),[](const Display& d){return d.id=="B";}),displays.end());++topologyEpoch;MapRefresh();
    Check(mapSelected=="B"&&SendMessageW(list,LB_GETCURSEL,0,0)==LB_ERR&&!MapTarget(false)&&ControlText(displayMap,MapDetail).find(L"disconnected")!=std::wstring::npos,"map disconnected selection stays explicit and never selects a peer");
    for(int dpi:{144,192,96}){RECT bounds{0,0,MulDiv(780,dpi,96),MulDiv(760,dpi,96)};SendMessageW(displayMap,WM_DPICHANGED,MAKEWPARAM(dpi,dpi),reinterpret_cast<LPARAM>(&bounds));Check(displayMap==window&&GetDlgItem(displayMap,MapList)==list&&mapLayout.dpi==dpi,"map DPI refresh retains native keyboard list and stable window");}
    for(int mode:{0,1,2}){testTheme=mode;ReadUiTheme();RefreshDisplayMapTheme();Check(displayMap==window&&mapSelected=="B"&&mapFont,"map light dark and high contrast retain disconnected target");}testTheme=-1;ReadUiTheme();
    CloseDisplayMap();Check(!displayMap&&!mapFont&&mapIds.empty()&&mapFontDeleted,"map closes and releases font/window snapshot resources");SetupSessions();return 1;
}
