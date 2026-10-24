#include "../SDK/ConnectionTrace.h"
#include <windows.h>
#include <iostream>
int main(){
 std::cout<<"Read-only connection metadata for 10 seconds; run as administrator.\n";
 try{
  ConnectionTrace::Session capture;
  capture.Start();
  const auto until=GetTickCount64()+10000;
  while(GetTickCount64()<until){
   for(const auto& event:capture.Read())
    std::wcout<<ConnectionTrace::Stage(event)<<L" PID "<<event.ProcessId<<L" -> "<<ConnectionTrace::Address(event,true)<<L'\n';
   Sleep(20); // User-mode polling only; no connection is held for this delay.
  }
 }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
 // The session destructor removes its WFP filters and closes the sensor owner.
}
