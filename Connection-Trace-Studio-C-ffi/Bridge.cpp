#include "SDK/ConnectionTrace.h"
#include "WireTypes.h"
#include <map>
struct ProductSession{

std::unique_ptr<ConnectionTrace::Session> session;
ConnectionTrace::Session& Get(){if(!session)session=std::make_unique<ConnectionTrace::Session>();return *session;}
ConnectionTrace::Correlator correlator;

Json Invoke(const std::string& op,const Json& a){

if(op=="check"){auto value=FromWire<CT_STATUS>(a.at("status"));return {{"valid",ConnectionTrace::Validate(value)},{"value",EncodeWire(value)}};}
if(op=="start"){Get().Start(a.value("include_paths",false));return Json::object();}
if(op=="stop"){if(session)session->Stop();return Json::object();}
if(op=="status")return EncodeWire(Get().Status());
if(op=="read")return VectorWire(Get().Read());
if(op=="format_event"){auto e=FromWire<CT_EVENT>(a.at("event"));Require(e.PathLength<=CT_PATH_CAPACITY,"Application path exceeds wire capacity");return {{"application",Utf8(ConnectionTrace::Application(e))},{"local_address",Utf8(ConnectionTrace::Address(e,false))},{"remote_address",Utf8(ConnectionTrace::Address(e,true))},{"stage",Utf8(ConnectionTrace::Stage(e))}};}
if(op=="correlate"){auto e=FromWire<CT_EVENT>(a.at("event"));correlator.Observe(e);Json list=Json::array();for(const auto& v:correlator.Entries())list.push_back({{"endpoint",std::to_string(v.endpoint)},{"pid",std::to_string(v.pid)},{"authorize",std::to_string(v.authorize)},{"established",std::to_string(v.established)},{"closed",std::to_string(v.closed)},{"missing_start",v.missing_start},{"pid_known",v.pid_known},{"incomplete",v.incomplete}});return {{"entries",list},{"evicted",std::to_string(correlator.Evicted())}};}

throw SdkError(50,"Unknown operation");}
};
static Runtime<ProductSession> runtime;
extern "C" __declspec(dllexport) unsigned connection_trace_studio_json_open(std::uint64_t* id)noexcept{return runtime.Open(id);}
extern "C" __declspec(dllexport) unsigned connection_trace_studio_json_close(std::uint64_t id)noexcept{return runtime.Close(id);}
extern "C" __declspec(dllexport) char* connection_trace_studio_json_call(std::uint64_t id,const char* request,std::uint32_t length)noexcept{return runtime.Call(id,request,length);}
extern "C" __declspec(dllexport) void connection_trace_studio_json_free(char* response)noexcept{delete[] response;}
