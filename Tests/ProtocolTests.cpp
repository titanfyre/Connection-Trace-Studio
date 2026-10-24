#include "../SDK/ConnectionTrace.h"
#include "../App/Json.h"
#include <iostream>
#include <stdexcept>
#include <random>
#include <cstring>
namespace {unsigned checks=0;void Require(bool value){++checks;if(!value)throw std::runtime_error("Protocol/correlation assertion "+std::to_string(checks));}
CT_EVENT Event(CT_U64 seq,CT_U64 endpoint,CT_U8 stage){CT_EVENT e{};e.Sequence=seq;e.TimestampQpc=seq*100;e.EndpointId=endpoint;e.ProcessId=42;e.Flags=CT_FLAG_PID|CT_FLAG_ENDPOINT;e.Family=4;e.Stage=stage;return e;}}
int main(int argc,char** argv){
 if(argc==2&&std::string(argv[1])=="--json-endpoint-samples"){
  std::cout<<'['<<ConnectionTraceExport::Endpoint(0)<<','<<ConnectionTraceExport::Endpoint(9007199254740993ull)<<','<<ConnectionTraceExport::Endpoint(~CT_U64(0))<<"]\n";return 0;
 }
 try{
 Require(ConnectionTraceExport::Endpoint(0)=="\"0x0000000000000000\"");
 Require(ConnectionTraceExport::Endpoint(9007199254740993ull)=="\"0x0020000000000001\"");
 Require(ConnectionTraceExport::Endpoint(~CT_U64(0))=="\"0xFFFFFFFFFFFFFFFF\"");
 CT_STATUS status{CT_MAGIC,CT_VERSION,sizeof(CT_STATUS),1,10000000,9,1,8,CT_RING_CAPACITY,0,0};Require(ConnectionTrace::Validate(status));status.Options=2;Require(!ConnectionTrace::Validate(status));status.Options=0;status.Reserved=1;Require(!ConnectionTrace::Validate(status));status.Reserved=0;status.Available=CT_RING_CAPACITY+1;Require(!ConnectionTrace::Validate(status));status.Available=0;status.QpcFrequency=0;Require(!ConnectionTrace::Validate(status));
 CT_BATCH batch{};batch.Magic=CT_MAGIC;batch.Version=CT_VERSION;batch.Size=sizeof(batch);batch.Count=2;batch.Events[0]=Event(1,7,1);batch.Events[1]=Event(2,7,2);Require(ConnectionTrace::Validate(batch,sizeof(batch)));Require(!ConnectionTrace::Validate(batch,sizeof(batch)-1));
 auto valid=batch;batch.Count=CT_READ_CAPACITY+1;Require(!ConnectionTrace::Validate(batch,sizeof(batch)));batch=valid;batch.Events[1].Sequence=1;Require(!ConnectionTrace::Validate(batch,sizeof(batch)));batch=valid;batch.Events[0].PathLength=CT_PATH_CAPACITY;Require(!ConnectionTrace::Validate(batch,sizeof(batch)));batch=valid;batch.Events[0].Application[0]=L'x';Require(!ConnectionTrace::Validate(batch,sizeof(batch)));batch=valid;batch.Events[0].RemoteAddress[15]=1;Require(!ConnectionTrace::Validate(batch,sizeof(batch)));batch=valid;batch.Events[0].Reserved=1;Require(!ConnectionTrace::Validate(batch,sizeof(batch)));batch=valid;batch.Events[0].Flags|=0x80000000u;Require(!ConnectionTrace::Validate(batch,sizeof(batch)));
 ConnectionTrace::Correlator correlator;auto e=Event(1,77,1);Require(correlator.Observe(e)->authorize==100);e=Event(2,77,2);Require(correlator.Observe(e)->established==200);e=Event(3,77,3);Require(correlator.Observe(e)->closed==300);e=Event(4,77,1);Require(correlator.Observe(e)->authorize==400&&correlator.Entries().size()==1);e=Event(5,88,2);Require(correlator.Observe(e)->missing_start);e=Event(6,99,1);e.Flags|=CT_FLAG_REAUTHORIZE;Require(correlator.Observe(e)->missing_start&&correlator.Observe(e)->authorize==0);e.Flags=0;Require(correlator.Observe(e)==nullptr);
 for(unsigned i=0;i<50000;++i){e=Event(100+i,1000+i,2);Require(correlator.Observe(e)!=nullptr&&correlator.Entries().size()<=2048);}Require(correlator.Evicted()>0);
 ConnectionTrace::Correlator reuse;e=Event(1,9,1);reuse.Observe(e);e=Event(2,9,1);auto l=reuse.Observe(e);Require(l->authorize==200&&l->incomplete);e=Event(3,9,2);e.ProcessId=77;l=reuse.Observe(e);Require(l->pid==77&&l->missing_start&&l->incomplete);
 ConnectionTrace::Correlator closedOnly;e=Event(1,10,3);closedOnly.Observe(e);e=Event(2,10,1);l=closedOnly.Observe(e);Require(l->closed==0&&l->authorize==200&&!l->missing_start);
 ConnectionTrace::Correlator pidLater;e=Event(1,11,1);e.Flags&=~CT_FLAG_PID;e.ProcessId=0;pidLater.Observe(e);e=Event(2,11,2);l=pidLater.Observe(e);Require(l->pid_known&&l->pid==42);
 e=Event(4,11,3);l=pidLater.Observe(e);Require(l->incomplete);e=Event(1,12,1);l=pidLater.Observe(e);Require(pidLater.Entries().size()==1&&!l->incomplete);
 batch=valid;batch.Events[0].Flags|=CT_FLAG_PORTS;batch.Events[0].Protocol=1;Require(!ConnectionTrace::Validate(batch,sizeof(batch)));batch.Events[0].Flags|=CT_FLAG_PROTOCOL;Require(!ConnectionTrace::Validate(batch,sizeof(batch)));batch.Events[0].Protocol=6;Require(ConnectionTrace::Validate(batch,sizeof(batch)));batch.Events[0].Protocol=17;Require(ConnectionTrace::Validate(batch,sizeof(batch)));batch=valid;batch.Events[0].LocalPort=20;Require(!ConnectionTrace::Validate(batch,sizeof(batch)));batch=valid;batch.Events[0].Flags|=CT_FLAG_PATH_TRUNCATED;Require(!ConnectionTrace::Validate(batch,sizeof(batch)));batch=valid;batch.Events[1].Flags|=CT_FLAG_REAUTHORIZE;Require(!ConnectionTrace::Validate(batch,sizeof(batch)));
 // Fuzz the actual public decoder with bounded arbitrary input, including paths.
 std::mt19937 random(0x43545243u);for(unsigned n=0;n<20000;++n){batch=valid;auto* bytes=reinterpret_cast<unsigned char*>(&batch);for(unsigned i=0;i<8;++i)bytes[random()%sizeof(batch)]=static_cast<unsigned char>(random());bool accepted=ConnectionTrace::Validate(batch,sizeof(batch));if(accepted)for(unsigned i=0;i<batch.Count;++i){Require(batch.Events[i].PathLength<CT_PATH_CAPACITY);(void)ConnectionTrace::Application(batch.Events[i]);(void)ConnectionTrace::Address(batch.Events[i],true);}Require(true);}
 std::cout<<checks<<" assertions passed; no driver or WFP session was opened.\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
