#include "ConnectionTrace.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <setupapi.h>
#include <fwpmu.h>
#include <algorithm>
#include <array>
#include <stdexcept>
#include <system_error>
#include <cstring>
namespace ConnectionTrace {
namespace {
const GUID Interface={0xf08a1f3d,0xa292,0x47da,{0x94,0xab,0x6c,0xb4,0xf8,0xc7,0xca,0x38}};
const GUID Provider={0xe56dc010,0x6dbd,0x431c,{0x8c,0x9d,0xca,0x36,0x58,0x4f,0xf0,0x51}};
const GUID Sublayer={0xe56dc011,0x6dbd,0x431c,{0x8c,0x9d,0xca,0x36,0x58,0x4f,0xf0,0x51}};
const GUID Keys[6]={
 {0xe56dc000,0x6dbd,0x431c,{0x8c,0x9d,0xca,0x36,0x58,0x4f,0xf0,0x51}},
 {0xe56dc001,0x6dbd,0x431c,{0x8c,0x9d,0xca,0x36,0x58,0x4f,0xf0,0x51}},
 {0xe56dc002,0x6dbd,0x431c,{0x8c,0x9d,0xca,0x36,0x58,0x4f,0xf0,0x51}},
 {0xe56dc003,0x6dbd,0x431c,{0x8c,0x9d,0xca,0x36,0x58,0x4f,0xf0,0x51}},
 {0xe56dc004,0x6dbd,0x431c,{0x8c,0x9d,0xca,0x36,0x58,0x4f,0xf0,0x51}},
 {0xe56dc005,0x6dbd,0x431c,{0x8c,0x9d,0xca,0x36,0x58,0x4f,0xf0,0x51}}
};
const GUID* Layers[6]={&FWPM_LAYER_ALE_AUTH_CONNECT_V4,&FWPM_LAYER_ALE_AUTH_CONNECT_V6,
 &FWPM_LAYER_ALE_FLOW_ESTABLISHED_V4,&FWPM_LAYER_ALE_FLOW_ESTABLISHED_V6,
 &FWPM_LAYER_ALE_ENDPOINT_CLOSURE_V4,&FWPM_LAYER_ALE_ENDPOINT_CLOSURE_V6};
[[noreturn]] void Fail(DWORD e,const char* stage){throw std::runtime_error(std::string(stage)+": "+Session::Error(e));}
void Check(DWORD e,const char* stage){if(e)Fail(e,stage);}
HANDLE Open(){
 HDEVINFO devices=SetupDiGetClassDevsW(&Interface,nullptr,nullptr,DIGCF_DEVICEINTERFACE|DIGCF_PRESENT);
 if(devices==INVALID_HANDLE_VALUE)Fail(GetLastError(),"Find driver interface");
 struct Cleanup{HDEVINFO value;~Cleanup(){SetupDiDestroyDeviceInfoList(value);}} cleanup{devices};
 SP_DEVICE_INTERFACE_DATA iface{};iface.cbSize=sizeof(iface);
 if(!SetupDiEnumDeviceInterfaces(devices,nullptr,&Interface,0,&iface))Fail(GetLastError(),"Driver not installed or not running");
 DWORD size=0;SetupDiGetDeviceInterfaceDetailW(devices,&iface,nullptr,0,&size,nullptr);
 if(size<sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W)||size>65536)Fail(ERROR_INVALID_DATA,"Interface size");
 std::vector<unsigned char> storage(size);auto detail=reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_W*>(storage.data());detail->cbSize=sizeof(*detail);
 if(!SetupDiGetDeviceInterfaceDetailW(devices,&iface,detail,size,nullptr,nullptr))Fail(GetLastError(),"Read driver interface");
 HANDLE device=CreateFileW(detail->DevicePath,GENERIC_READ|GENERIC_WRITE,0,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
 if(device==INVALID_HANDLE_VALUE)Fail(GetLastError(),"Open sensor (administrator rights and one active instance required)");
 return device;
}
void Control(HANDLE device,DWORD code,void* output,DWORD size,CT_U32 options=0){
 CT_REQUEST input{CT_MAGIC,CT_VERSION,sizeof(CT_REQUEST),options};DWORD received=0;
 if(!DeviceIoControl(device,code,&input,sizeof(input),output,size,&received,nullptr))Fail(GetLastError(),"Driver request");
 if(received!=size)Fail(ERROR_INVALID_DATA,"Driver response length");
}
}
bool Validate(const CT_STATUS& s) noexcept{return s.Magic==CT_MAGIC&&s.Version==CT_VERSION&&s.Size==sizeof(s)&&s.Active<=1&&s.Capacity==CT_RING_CAPACITY&&s.Available<=s.Capacity&&s.QpcFrequency!=0&&s.Dropped<=s.Produced&&!(s.Options&~CT_OPTION_INCLUDE_PATHS)&&!s.Reserved;}
bool Validate(const CT_BATCH& b,std::size_t bytes) noexcept{
 if(bytes!=sizeof(b)||b.Magic!=CT_MAGIC||b.Version!=CT_VERSION||b.Size!=sizeof(b)||b.Count>CT_READ_CAPACITY)return false;
 CT_U64 previous=0;
 for(CT_U32 i=0;i<b.Count;++i){const auto& e=b.Events[i];if(!e.Sequence||e.Sequence<=previous||!e.TimestampQpc||e.Family!=4&&e.Family!=6||e.Stage<1||e.Stage>3||e.Reserved||e.Reserved2||e.Flags&~CT_FLAGS_ALL||e.PathLength>=CT_PATH_CAPACITY)return false;
  if(!(e.Flags&CT_FLAG_PATH)&&e.PathLength)return false;
  if((e.Flags&CT_FLAG_PATH_TRUNCATED)&&!(e.Flags&CT_FLAG_PATH))return false;
  if((e.Flags&CT_FLAG_REAUTHORIZE)&&e.Stage!=CT_STAGE_AUTHORIZE)return false;
  if((e.Flags&CT_FLAG_PORTS)&&(!(e.Flags&CT_FLAG_PROTOCOL)||(e.Protocol!=6&&e.Protocol!=17)))return false;
  if(!(e.Flags&CT_FLAG_PORTS)&&(e.LocalPort||e.RemotePort))return false;
  if(!(e.Flags&CT_FLAG_PROTOCOL)&&e.Protocol)return false;
  if(!(e.Flags&CT_FLAG_PID)&&e.ProcessId)return false;
  if(!(e.Flags&CT_FLAG_ENDPOINT)&&e.EndpointId)return false;
  if(e.Application[e.PathLength]!=0)return false;
  if(e.Family==4)for(unsigned n=4;n<16;++n)if(e.LocalAddress[n]||e.RemoteAddress[n])return false;
  previous=e.Sequence;
 }
 return true;
}
const wchar_t* Stage(const CT_EVENT& e) noexcept{switch(e.Stage){case 1:return e.Flags&CT_FLAG_REAUTHORIZE?L"Reauthorize":L"Authorize";case 2:return L"Established / authorized";case 3:return L"Endpoint closed";default:return L"Unknown";}}
std::wstring Application(const CT_EVENT& e){if(e.PathLength>=CT_PATH_CAPACITY)return L"Invalid path";std::wstring value;value.reserve(e.PathLength);for(unsigned i=0;i<e.PathLength;++i)value.push_back(static_cast<wchar_t>(e.Application[i]));return value;}
std::wstring Address(const CT_EVENT& e,bool remote){
 const CT_U32 flag=remote?CT_FLAG_REMOTE_ADDRESS:CT_FLAG_LOCAL_ADDRESS;if(!(e.Flags&flag))return L"unavailable";
 wchar_t address[INET6_ADDRSTRLEN]{};const auto* bytes=remote?e.RemoteAddress:e.LocalAddress;
 if(!InetNtopW(e.Family==4?AF_INET:AF_INET6,const_cast<CT_U8*>(bytes),address,INET6_ADDRSTRLEN))return L"invalid";
 std::wstring result=address;
 if(e.Flags&CT_FLAG_PORTS)result=(e.Family==6?L"["+result+L"]":result)+L":"+std::to_wstring(remote?e.RemotePort:e.LocalPort);
 return result;
}
const Lifetime* Correlator::Observe(const CT_EVENT& e){
 if(!e.Sequence)return nullptr;
 bool gap=last_sequence_?e.Sequence>last_sequence_+1:e.Sequence>1;
 if(last_sequence_&&e.Sequence<=last_sequence_){evicted_+=entries_.size();entries_.clear();gap=e.Sequence!=1;}
 last_sequence_=e.Sequence;
 if(gap)for(auto& entry:entries_)entry.incomplete=true;
 if(!(e.Flags&CT_FLAG_ENDPOINT)||!e.EndpointId)return nullptr;
 auto it=std::find_if(entries_.begin(),entries_.end(),[&](const Lifetime& l){return l.endpoint==e.EndpointId;});
 const bool pidKnown=bool(e.Flags&CT_FLAG_PID);
 const bool fresh=e.Stage==CT_STAGE_AUTHORIZE&&!(e.Flags&CT_FLAG_REAUTHORIZE);
 bool incomplete=gap;
 if(it!=entries_.end()&&(fresh||(it->pid_known&&pidKnown&&it->pid!=e.ProcessId))){
  incomplete=incomplete||!it->closed;entries_.erase(it);it=entries_.end();
 }
 if(it==entries_.end()){
  if(entries_.size()==2048){entries_.erase(entries_.begin());++evicted_;}
  entries_.push_back({e.EndpointId,e.ProcessId,0,0,0,!fresh,pidKnown,incomplete});it=entries_.end()-1;
 }
 if(pidKnown&&!it->pid_known){it->pid=e.ProcessId;it->pid_known=true;}
 if(e.Stage==CT_STAGE_AUTHORIZE&&!(e.Flags&CT_FLAG_REAUTHORIZE)&&!it->authorize)it->authorize=e.TimestampQpc;
 if(e.Stage==CT_STAGE_ESTABLISHED&&!it->established)it->established=e.TimestampQpc;
 if(e.Stage==CT_STAGE_CLOSED)it->closed=e.TimestampQpc;
 if((it->authorize&&it->established&&it->established<it->authorize)||(it->established&&it->closed&&it->closed<it->established)||(it->authorize&&it->closed&&it->closed<it->authorize))it->incomplete=true;
 return &*it;
}
struct Session::State{
 HANDLE device=INVALID_HANDLE_VALUE,engine=nullptr;bool active=false;ULONGLONG nextProbe=0;
 void Probe(){
  if(!active||GetTickCount64()<nextProbe)return;
  FWPM_PROVIDER0* provider=nullptr;DWORD result=FwpmProviderGetByKey0(engine,&Provider,&provider);
  if(provider)FwpmFreeMemory0(reinterpret_cast<void**>(&provider));
  Check(result,"Capture WFP session was lost; stop and start capture again");
  nextProbe=GetTickCount64()+1000;
 }
};
Session::Session():state_(std::make_unique<State>()){}
Session::~Session(){Stop();}
void Session::Start(bool includePaths){
 if(state_->active)throw std::runtime_error("Capture is already active.");
 try{
  state_->device=Open();Control(state_->device,CT_IOCTL_START,nullptr,0,includePaths?CT_OPTION_INCLUDE_PATHS:0);
  FWPM_SESSION0 session{};session.flags=FWPM_SESSION_FLAG_DYNAMIC;session.txnWaitTimeoutInMSec=1000;
  Check(FwpmEngineOpen0(nullptr,RPC_C_AUTHN_WINNT,nullptr,&session,&state_->engine),"Open dynamic WFP session");
  Check(FwpmTransactionBegin0(state_->engine,0),"Begin WFP transaction");
  FWPM_PROVIDER0 provider{};provider.providerKey=Provider;provider.displayData.name=const_cast<wchar_t*>(L"Titan Connection Trace Studio");
  Check(FwpmProviderAdd0(state_->engine,&provider,nullptr),"Register own WFP provider");
  FWPM_SUBLAYER0 sublayer{};sublayer.subLayerKey=Sublayer;sublayer.providerKey=const_cast<GUID*>(&Provider);sublayer.displayData.name=const_cast<wchar_t*>(L"Titan read-only connection stages");sublayer.weight=0;
  Check(FwpmSubLayerAdd0(state_->engine,&sublayer,nullptr),"Register own WFP sublayer");
  for(unsigned i=0;i<6;++i){
   FWPM_CALLOUT0 callout{};callout.calloutKey=Keys[i];callout.providerKey=const_cast<GUID*>(&Provider);callout.applicableLayer=*Layers[i];callout.displayData.name=const_cast<wchar_t*>(L"Titan metadata inspection");
   Check(FwpmCalloutAdd0(state_->engine,&callout,nullptr,nullptr),"Add inspection callout");
   FWPM_FILTER0 filter{};filter.displayData.name=const_cast<wchar_t*>(L"Titan connection stage observation");filter.providerKey=const_cast<GUID*>(&Provider);filter.layerKey=*Layers[i];filter.subLayerKey=Sublayer;filter.weight.type=FWP_EMPTY;filter.action.type=FWP_ACTION_CALLOUT_INSPECTION;filter.action.calloutKey=Keys[i];
   // FLOW_ESTABLISHED exposes direction; closure contains socket lifetime metadata.
   FWPM_FILTER_CONDITION0 condition{};
   if(i==2||i==3){condition.fieldKey=FWPM_CONDITION_DIRECTION;condition.matchType=FWP_MATCH_EQUAL;condition.conditionValue.type=FWP_UINT32;condition.conditionValue.uint32=FWP_DIRECTION_OUTBOUND;filter.numFilterConditions=1;filter.filterCondition=&condition;}
   Check(FwpmFilterAdd0(state_->engine,&filter,nullptr,nullptr),"Add dynamic inspection filter");
  }
  Check(FwpmTransactionCommit0(state_->engine),"Commit WFP transaction");
  state_->active=true;state_->nextProbe=0;const auto status=Status();if(!status.Active)throw std::runtime_error("Driver did not enable capture.");
 }catch(...){Stop();throw;}
}
void Session::Stop() noexcept{
 if(state_->engine){FwpmEngineClose0(state_->engine);state_->engine=nullptr;}
 if(state_->device!=INVALID_HANDLE_VALUE){CT_REQUEST input{CT_MAGIC,CT_VERSION,sizeof(CT_REQUEST),0};DWORD received=0;DeviceIoControl(state_->device,CT_IOCTL_STOP,&input,sizeof(input),nullptr,0,&received,nullptr);CloseHandle(state_->device);state_->device=INVALID_HANDLE_VALUE;}
 state_->active=false;
}
CT_STATUS Session::Status() const{
 if(state_->device==INVALID_HANDLE_VALUE)throw std::runtime_error("Capture has not been started.");
 state_->Probe();
 CT_STATUS result{};Control(state_->device,CT_IOCTL_STATUS,&result,sizeof(result));if(!Validate(result))Fail(ERROR_INVALID_DATA,"Validate driver status");return result;
}
std::vector<CT_EVENT> Session::Read(){
 if(!state_->active)throw std::runtime_error("Capture is inactive.");
 state_->Probe();
 CT_BATCH batch{};Control(state_->device,CT_IOCTL_READ,&batch,sizeof(batch));if(!Validate(batch,sizeof(batch)))Fail(ERROR_INVALID_DATA,"Validate event batch");return {batch.Events,batch.Events+batch.Count};
}
std::string Session::Error(unsigned long code){
 // Locale-independent messages for common API errors; always preserve numeric code.
 const char* description="Windows operation failed";
 switch(code){case ERROR_ACCESS_DENIED:description="Access denied; run as administrator";break;case ERROR_FILE_NOT_FOUND:case ERROR_NO_MORE_ITEMS:description="Sensor driver not installed or not running";break;case ERROR_SHARING_VIOLATION:case ERROR_BUSY:description="Another capture instance owns the sensor";break;case ERROR_NOT_READY:description="Sensor is not ready";break;case ERROR_INVALID_DATA:description="Sensor response failed validation";break;case RPC_S_SERVER_UNAVAILABLE:description="Windows Base Filtering Engine is unavailable";break;}
 return std::string(description)+" (Windows error "+std::to_string(code)+")";
}
}
