// Compatible desired-behavior probe: compile the unchanged 0.2.1 source and the
// current source with identical input adapters. The old binary must run and fail
// at the named missed-wake assertion, never merely fail to compile.
#include "nativewake-input-fixture.h"
#define DAC_HARNESS
#ifndef DAC_WAKE_SOURCE
#define DAC_WAKE_SOURCE "../src/dac.cpp"
#endif
#include DAC_WAKE_SOURCE
#include <cstdlib>
using namespace dac;
void Require(bool condition,const char* message){if(!condition){fprintf(stderr,"FAIL %s\n",message);std::_Exit(1);}}
LRESULT Probe(){
    wake_fixture::currentPoint={-400,100};wake_fixture::currentMonitor=reinterpret_cast<HMONITOR>(1);
    for(DWORD age:{0u,300u}){injectedObservations=true;controller=Controller{};controller.config.media=false;controller.config.perInput=true;
    auto now=GetTickCount64();displays={{"A","",L"",reinterpret_cast<HMONITOR>(1),{-800,-200,0,400},true},{"B","",L"",reinterpret_cast<HMONITOR>(2),{0,-200,800,400},true}};
    controller.Topology({"A","B"},now-20000);controller.Manual("A",now-7000,false,-1);controller.Manual("B",now-7000,false,-1);Reconcile();Require(runs.size()==2,"hidden native sessions exist");auto other=controller.nodes["B"];
    wake_fixture::packet={};wake_fixture::packet.header.dwType=RIM_TYPEMOUSE;wake_fixture::packet.data.mouse.lLastX=1;wake_fixture::message={};wake_fixture::message.time=static_cast<DWORD>(now-age);wake_fixture::message.pt={-400,100};wake_fixture::enabled=true;
    injectedObservations=false;Input(reinterpret_cast<HRAWINPUT>(1));injectedObservations=true;wake_fixture::enabled=false;
    Require(!Running(controller.nodes["A"].state),age==0?"fresh positive control must wake intended existing native session":"queued 300-ms mouse movement must wake intended existing native session without second input");Require(controller.nodes["B"].generation==other.generation&&controller.nodes["B"].last==other.last,"unrelated native session and idle history preserved");
    printf("POSITIVE control queue-age=%lu ms: wake A and preserve B passed\n",age);fflush(stdout);Reset();Reconcile();}
    return 1;
}
int wmain(){Require(Initialize(),"initialization");DWORD_PTR result=0;Require(SendMessageTimeoutW(ui,WM_APP+101,0,reinterpret_cast<LPARAM>(Probe),SMTO_ABORTIFHUNG,3000,&result)&&result,"probe on UI owner");Shutdown();puts("NATIVEWAKE COMPATIBLE PROBE PASS: one queued movement wakes A; B generation/idle unchanged; hidden only");}
