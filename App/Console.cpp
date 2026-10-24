#include "../SDK/ConnectionTrace.h"
#include "Json.h"
#include <windows.h>
#include <iostream>
#include <charconv>
#include <atomic>
#include <filesystem>
#include <sstream>
#include <stdexcept>
namespace {std::atomic_bool stopping=false;BOOL WINAPI Stop(DWORD code){if(code==CTRL_C_EVENT||code==CTRL_BREAK_EVENT){stopping=true;return TRUE;}return FALSE;}}
namespace {
std::string Quote(const std::string& value){std::string result="\"";const char* hex="0123456789abcdef";for(unsigned char ch:value){switch(ch){case '\\':result+="\\\\";break;case '"':result+="\\\"";break;case '\n':result+="\\n";break;case '\r':result+="\\r";break;case '\t':result+="\\t";break;default:if(ch<32){result+="\\u00";result+=hex[ch>>4];result+=hex[ch&15];}else result+=static_cast<char>(ch);}}return result+'"';}
std::wstring ConsolePath(const std::wstring& text){std::wstring safe;const wchar_t* hex=L"0123456789ABCDEF";for(wchar_t ch:text){if(ch<32||ch==127||ch==L'|'){safe+=L"\\u";for(int shift=12;shift>=0;shift-=4)safe+=hex[(static_cast<unsigned>(ch)>>shift)&15];}else safe+=ch;}return safe;}
void NewFile(const std::filesystem::path& path,const std::string& text){
 HANDLE file=CreateFileW(path.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);if(file==INVALID_HANDLE_VALUE)throw std::runtime_error("Create new report: "+ConnectionTrace::Session::Error(GetLastError()));
 DWORD written=0;bool ok=text.size()<=MAXDWORD&&WriteFile(file,text.data(),static_cast<DWORD>(text.size()),&written,nullptr)&&written==text.size();DWORD error=GetLastError();CloseHandle(file);if(!ok)throw std::runtime_error("Write report: "+ConnectionTrace::Session::Error(error));
}
int SessionTest(const char* path){
 try{ConnectionTrace::Session session;for(bool option:{false,true,false}){session.Start(option);auto status=session.Status();if(status.Active!=1||status.Options!=(option?CT_OPTION_INCLUDE_PATHS:0))throw std::runtime_error("Active/options mismatch after Start/Stop cycle");session.Stop();}NewFile(std::filesystem::path(path),"{\"success\":true,\"options_checked\":[0,1,0],\"scope\":\"actual driver Start/Stop and dynamic WFP lifecycle\"}\n");return 0;}
 catch(const std::exception& e){std::cerr<<e.what()<<'\n';try{NewFile(std::filesystem::path(path),"{\"success\":false,\"detail\":"+Quote(e.what())+"}\n");}catch(...){ }return 1;}
}
int CaptureJson(unsigned seconds,const char* path,bool includePaths){
 try{
  const std::filesystem::path report(path);auto ready=report;ready+=L".ready";
  if(std::filesystem::exists(report)||std::filesystem::exists(ready))throw std::runtime_error("Report or readiness marker already exists; use a new output path");
  ConnectionTrace::Session session;session.Start(includePaths);auto status=session.Status();
  if(status.Active!=1||status.Options!=(includePaths?CT_OPTION_INCLUDE_PATHS:0))throw std::runtime_error("Driver capture options do not match the request");
  NewFile(ready,"ready\n");std::vector<CT_EVENT> events;events.reserve(16384);bool truncated=false;const auto until=GetTickCount64()+seconds*1000ull;
  while(!stopping&&GetTickCount64()<until){for(const auto& e:session.Read()){if(events.size()<16384)events.push_back(e);else truncated=true;}Sleep(10);}
  for(unsigned n=0;n<32;++n){auto batch=session.Read();if(batch.empty())break;for(const auto& e:batch){if(events.size()<16384)events.push_back(e);else truncated=true;}}
  status=session.Status();session.Stop();std::ostringstream json;json<<"{\"success\":true,\"scope\":\"observed WFP metadata; not end-to-end latency\",\"options\":"<<status.Options<<",\"produced\":"<<status.Produced<<",\"dropped\":"<<status.Dropped<<",\"report_truncated\":"<<(truncated?"true":"false")<<",\"qpc_frequency\":"<<status.QpcFrequency<<",\"events\":[";
  bool first=true;for(const auto& e:events){if(!first)json<<',';first=false;json<<"{\"sequence\":"<<e.Sequence<<",\"qpc\":"<<e.TimestampQpc<<",\"pid\":"<<e.ProcessId<<",\"endpoint\":"<<ConnectionTraceExport::Endpoint(e.EndpointId)<<",\"flags\":"<<e.Flags<<",\"family\":"<<unsigned(e.Family)<<",\"stage\":"<<unsigned(e.Stage)<<",\"protocol\":"<<unsigned(e.Protocol)<<",\"local_port\":"<<e.LocalPort<<",\"remote_port\":"<<e.RemotePort<<",\"path_length\":"<<e.PathLength<<'}';}
  json<<"]}\n";NewFile(report,json.str());std::cout<<"Capture report written.\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';try{NewFile(std::filesystem::path(path),"{\"success\":false,\"detail\":"+Quote(e.what())+"}\n");}catch(...){ }return 1;}
}
}
int main(int argc,char** argv){
 if(argc==3&&std::string(argv[1])=="--session-self-test")return SessionTest(argv[2]);
 const bool json=argc>=2&&std::string(argv[1])=="--capture-json";
 if(json){
  if((argc!=4&&argc!=5)||(argc==5&&std::string(argv[4])!="--include-paths")){std::cerr<<"Usage: --capture-json SECONDS NEW_REPORT.json [--include-paths]\n";return 2;}
  unsigned seconds=0;auto end=argv[2]+std::char_traits<char>::length(argv[2]);auto value=std::from_chars(argv[2],end,seconds);if(value.ec!=std::errc{}||value.ptr!=end||seconds<1||seconds>3600)return 2;
  SetConsoleCtrlHandler(Stop,TRUE);return CaptureJson(seconds,argv[3],argc==5);
 }
 if((argc!=3&&argc!=4)||std::string(argv[1])!="--capture"||(argc==4&&std::string(argv[3])!="--include-paths")){
  std::cout<<"Connection Trace Studio\nUsage: ConnectionTraceCLI.exe --capture SECONDS [--include-paths]\nRun as administrator. Paths are opt-in. Read-only metadata, no packet payload.\n";return 2;
 }
 unsigned seconds=0;auto end=argv[2]+std::char_traits<char>::length(argv[2]);auto value=std::from_chars(argv[2],end,seconds);
 if(value.ec!=std::errc{}||value.ptr!=end||seconds<1||seconds>3600){std::cerr<<"SECONDS must be between 1 and 3600.\n";return 2;}
 try{
  SetConsoleCtrlHandler(Stop,TRUE);ConnectionTrace::Session capture;capture.Start(argc==4);
  std::wcout<<L"Capture started. Ctrl+C stops safely. Local output only.\n";
  const auto deadline=GetTickCount64()+seconds*1000ull;
  while(!stopping&&GetTickCount64()<deadline){for(const auto& event:capture.Read())std::wcout<<event.Sequence<<L" | "<<ConnectionTrace::Stage(event)<<L" | PID "<<event.ProcessId<<L" | "<<ConnectionTrace::Address(event,true)<<L" | "<<ConsolePath(ConnectionTrace::Application(event))<<L'\n';Sleep(20);}
  const auto status=capture.Status();std::wcout<<L"Produced: "<<status.Produced<<L"; dropped: "<<status.Dropped<<L". This is not a complete packet capture.\n";
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
 return 0;
}
