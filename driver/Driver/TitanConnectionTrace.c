/* Original Titanfyre read-only ALE connection stage sensor. */
#include <ntddk.h>
#include <wdf.h>
#define NDIS630 1
#pragma warning(push)
#pragma warning(disable:4201)
#include <ndis.h>
#include <fwpsk.h>
#pragma warning(pop)
#include <initguid.h>
#include <fwpmk.h>
#include "../SDK/ConnectionTrace.h"
#include "Ring.h"

C_ASSERT(sizeof(CT_EVENT)==336);
C_ASSERT(sizeof(CT_STATUS)==56);

DEFINE_GUID(GUID_CT_INTERFACE,0xf08a1f3d,0xa292,0x47da,0x94,0xab,0x6c,0xb4,0xf8,0xc7,0xca,0x38);
static const GUID Keys[6]={
 {0xe56dc000,0x6dbd,0x431c,{0x8c,0x9d,0xca,0x36,0x58,0x4f,0xf0,0x51}},
 {0xe56dc001,0x6dbd,0x431c,{0x8c,0x9d,0xca,0x36,0x58,0x4f,0xf0,0x51}},
 {0xe56dc002,0x6dbd,0x431c,{0x8c,0x9d,0xca,0x36,0x58,0x4f,0xf0,0x51}},
 {0xe56dc003,0x6dbd,0x431c,{0x8c,0x9d,0xca,0x36,0x58,0x4f,0xf0,0x51}},
 {0xe56dc004,0x6dbd,0x431c,{0x8c,0x9d,0xca,0x36,0x58,0x4f,0xf0,0x51}},
 {0xe56dc005,0x6dbd,0x431c,{0x8c,0x9d,0xca,0x36,0x58,0x4f,0xf0,0x51}}
};
typedef struct CT_DEVICE {
    KSPIN_LOCK Lock;
    EX_RUNDOWN_REF Rundown;
    WDFFILEOBJECT Owner;
    CT_EVENT Ring[CT_RING_CAPACITY];
    CT_U64 Frequency,Generation,TokenSeed;
    CT_U32 Options;
    CT_RING_INDEX Index;
    
    UINT32 Callouts[6];
    BOOLEAN Registered[6],Active,Online;
} CT_DEVICE;
WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(CT_DEVICE, Context)
typedef struct CT_FILE { BOOLEAN Closing; } CT_FILE;
WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(CT_FILE, FileContext)
static PVOID volatile Singleton;

DRIVER_INITIALIZE DriverEntry;
EVT_WDF_DRIVER_DEVICE_ADD DeviceAdd;
EVT_WDF_OBJECT_CONTEXT_CLEANUP DeviceCleanup;
EVT_WDF_FILE_CLEANUP FileCleanup;
EVT_WDF_IO_QUEUE_IO_DEVICE_CONTROL DeviceControl;

static VOID Continue(FWPS_CLASSIFY_OUT0* result){
    if(result->rights & FWPS_RIGHT_ACTION_WRITE)result->actionType=FWP_ACTION_CONTINUE;
}
static BOOLEAN U32(const FWPS_INCOMING_VALUES0* v,UINT32 index,UINT32* out){
    if(index>=v->valueCount||v->incomingValue[index].value.type!=FWP_UINT32)return FALSE;
    *out=v->incomingValue[index].value.uint32;return TRUE;
}
static BOOLEAN U16(const FWPS_INCOMING_VALUES0* v,UINT32 index,CT_U16* out){
    if(index>=v->valueCount||v->incomingValue[index].value.type!=FWP_UINT16)return FALSE;
    *out=v->incomingValue[index].value.uint16;return TRUE;
}
static VOID Address(const FWPS_INCOMING_VALUES0* v,UINT32 index,BOOLEAN v6,CT_U8* out,CT_U32 flag,CT_EVENT* e){
    const FWP_VALUE0* value;
    if(index>=v->valueCount)return;
    value=&v->incomingValue[index].value;
    if(v6){
        if(value->type!=FWP_BYTE_ARRAY16_TYPE||value->byteArray16==NULL)return;
        RtlCopyMemory(out,value->byteArray16->byteArray16,16);
    }else{
        UINT32 a;
        if(value->type!=FWP_UINT32)return;
        a=value->uint32;out[0]=(UCHAR)(a>>24);out[1]=(UCHAR)(a>>16);out[2]=(UCHAR)(a>>8);out[3]=(UCHAR)a;
    }
    e->Flags|=flag;
}
static VOID NTAPI Classify(const FWPS_INCOMING_VALUES0* fixed,const FWPS_INCOMING_METADATA_VALUES0* meta,
    VOID* layerData,const VOID* classifyContext,const FWPS_FILTER1* filter,UINT64 flowContext,FWPS_CLASSIFY_OUT0* result){
    WDFDEVICE device;
    CT_DEVICE* c;
    CT_EVENT event;
    UINT32 local=MAXULONG,remote=MAXULONG,lp=MAXULONG,rp=MAXULONG,protocol=MAXULONG,flags=MAXULONG;
    BOOLEAN v6=FALSE;
    KIRQL old;
    CT_U64 generation;
    CT_U64 tokenSeed;
    CT_U32 options;
    UNREFERENCED_PARAMETER(layerData);UNREFERENCED_PARAMETER(classifyContext);
    UNREFERENCED_PARAMETER(filter);UNREFERENCED_PARAMETER(flowContext);
    Continue(result);
    device=(WDFDEVICE)InterlockedCompareExchangePointer(&Singleton,NULL,NULL);
    if(device==NULL)return;
    c=Context(device);
    if(!ExAcquireRundownProtection(&c->Rundown))return;
    KeAcquireSpinLock(&c->Lock,&old);
    if(!c->Active){KeReleaseSpinLock(&c->Lock,old);ExReleaseRundownProtection(&c->Rundown);return;}
    generation=c->Generation;
    tokenSeed=c->TokenSeed;options=c->Options;
    KeReleaseSpinLock(&c->Lock,old);
    RtlZeroMemory(&event,sizeof(event));
    event.TimestampQpc=(CT_U64)KeQueryPerformanceCounter(NULL).QuadPart;
    switch(fixed->layerId){
#define CT_FIELDS(prefix) local=prefix##_IP_LOCAL_ADDRESS;remote=prefix##_IP_REMOTE_ADDRESS;lp=prefix##_IP_LOCAL_PORT;rp=prefix##_IP_REMOTE_PORT;protocol=prefix##_IP_PROTOCOL
    case FWPS_LAYER_ALE_AUTH_CONNECT_V4:event.Stage=CT_STAGE_AUTHORIZE;CT_FIELDS(FWPS_FIELD_ALE_AUTH_CONNECT_V4);flags=FWPS_FIELD_ALE_AUTH_CONNECT_V4_FLAGS;break;
    case FWPS_LAYER_ALE_AUTH_CONNECT_V6:event.Stage=CT_STAGE_AUTHORIZE;v6=TRUE;CT_FIELDS(FWPS_FIELD_ALE_AUTH_CONNECT_V6);flags=FWPS_FIELD_ALE_AUTH_CONNECT_V6_FLAGS;break;
    case FWPS_LAYER_ALE_FLOW_ESTABLISHED_V4:event.Stage=CT_STAGE_ESTABLISHED;CT_FIELDS(FWPS_FIELD_ALE_FLOW_ESTABLISHED_V4);break;
    case FWPS_LAYER_ALE_FLOW_ESTABLISHED_V6:event.Stage=CT_STAGE_ESTABLISHED;v6=TRUE;CT_FIELDS(FWPS_FIELD_ALE_FLOW_ESTABLISHED_V6);break;
    case FWPS_LAYER_ALE_ENDPOINT_CLOSURE_V4:event.Stage=CT_STAGE_CLOSED;CT_FIELDS(FWPS_FIELD_ALE_ENDPOINT_CLOSURE_V4);break;
    case FWPS_LAYER_ALE_ENDPOINT_CLOSURE_V6:event.Stage=CT_STAGE_CLOSED;v6=TRUE;CT_FIELDS(FWPS_FIELD_ALE_ENDPOINT_CLOSURE_V6);break;
    default:ExReleaseRundownProtection(&c->Rundown);return;
#undef CT_FIELDS
    }
    event.Family=v6?6:4;
    if(FWPS_IS_METADATA_FIELD_PRESENT(meta,FWPS_METADATA_FIELD_PROCESS_ID)){event.ProcessId=meta->processId;event.Flags|=CT_FLAG_PID;}
    if(FWPS_IS_METADATA_FIELD_PRESENT(meta,FWPS_METADATA_FIELD_TRANSPORT_ENDPOINT_HANDLE)){
        CT_U64 token=meta->transportEndpointHandle^tokenSeed;
        /* Per-capture pseudonym, not a pointer, cryptographic identity or license. */
        token=(token^(token>>30))*0xbf58476d1ce4e5b9ull;
        token=(token^(token>>27))*0x94d049bb133111ebull;
        token=token^(token>>31);
        event.EndpointId=token?token:1;event.Flags|=CT_FLAG_ENDPOINT;
    }
    if((options&CT_OPTION_INCLUDE_PATHS)&&FWPS_IS_METADATA_FIELD_PRESENT(meta,FWPS_METADATA_FIELD_PROCESS_PATH)&&meta->processPath&&meta->processPath->data){
        UINT32 length=meta->processPath->size/sizeof(CT_U16);
        if(length>CT_PATH_CAPACITY-1){length=CT_PATH_CAPACITY-1;event.Flags|=CT_FLAG_PATH_TRUNCATED;}
        RtlCopyMemory(event.Application,meta->processPath->data,length*sizeof(CT_U16));
        while(length&&event.Application[length-1]==0)--length;
        event.PathLength=(CT_U16)length;event.Flags|=CT_FLAG_PATH;
    }
    Address(fixed,local,v6,event.LocalAddress,CT_FLAG_LOCAL_ADDRESS,&event);
    Address(fixed,remote,v6,event.RemoteAddress,CT_FLAG_REMOTE_ADDRESS,&event);
    if(protocol<fixed->valueCount&&fixed->incomingValue[protocol].value.type==FWP_UINT8){event.Protocol=fixed->incomingValue[protocol].value.uint8;event.Flags|=CT_FLAG_PROTOCOL;}
    /* At ALE layers ICMP type/code alias these fields. Export real transport
       ports only when a valid TCP/UDP protocol and both ports are available. */
    if((event.Flags&CT_FLAG_PROTOCOL)&&(event.Protocol==6||event.Protocol==17)){
        CT_U16 localPort,remotePort;
        if(U16(fixed,lp,&localPort)&&U16(fixed,rp,&remotePort)){
            event.LocalPort=localPort;event.RemotePort=remotePort;event.Flags|=CT_FLAG_PORTS;
        }
    }
    {UINT32 value;if(U32(fixed,flags,&value)&&(value&FWP_CONDITION_FLAG_IS_REAUTHORIZE))event.Flags|=CT_FLAG_REAUTHORIZE;}
    KeAcquireSpinLock(&c->Lock,&old);
    if(c->Active&&c->Generation==generation){
        ULONG slot=CtRingPush(&c->Index);
        event.Sequence=c->Index.Produced;
        c->Ring[slot]=event;
    }
    KeReleaseSpinLock(&c->Lock,old);
    ExReleaseRundownProtection(&c->Rundown);
}
static NTSTATUS NTAPI Notify(FWPS_CALLOUT_NOTIFY_TYPE type,const GUID* key,const FWPS_FILTER1* filter){
    UNREFERENCED_PARAMETER(type);UNREFERENCED_PARAMETER(key);UNREFERENCED_PARAMETER(filter);return STATUS_SUCCESS;
}
VOID FileCleanup(WDFFILEOBJECT file){
    CT_DEVICE* c=Context(WdfFileObjectGetDevice(file));KIRQL old;
    KeAcquireSpinLock(&c->Lock,&old);FileContext(file)->Closing=TRUE;
    if(c->Owner==file){c->Active=FALSE;c->Owner=NULL;c->Options=0;c->Index.Head=c->Index.Count=0;}
    KeReleaseSpinLock(&c->Lock,old);
}
VOID DeviceControl(WDFQUEUE queue,WDFREQUEST request,size_t output,size_t input,ULONG code){
    CT_DEVICE* c=Context(WdfIoQueueGetDevice(queue));
    CT_REQUEST* header=NULL;
    CT_STATUS* status=NULL;
    CT_BATCH* batch=NULL;
    NTSTATUS result=STATUS_INVALID_DEVICE_REQUEST;
    size_t bytes=0;
    WDFFILEOBJECT owner=WdfRequestGetFileObject(request);
    KIRQL old;
    if(input!=sizeof(CT_REQUEST)){WdfRequestComplete(request,STATUS_INFO_LENGTH_MISMATCH);return;}
    result=WdfRequestRetrieveInputBuffer(request,sizeof(CT_REQUEST),(PVOID*)&header,NULL);
    if(!NT_SUCCESS(result)){WdfRequestComplete(request,result);return;}
    if(header->Magic!=CT_MAGIC||header->Version!=CT_VERSION||header->Size!=sizeof(CT_REQUEST)||
       (code==CT_IOCTL_START?(header->Options&~CT_OPTION_INCLUDE_PATHS):header->Options)){
        WdfRequestComplete(request,STATUS_INVALID_PARAMETER);return;
    }
    if(code==CT_IOCTL_STATUS){
        if(output!=sizeof(CT_STATUS)){WdfRequestComplete(request,STATUS_INFO_LENGTH_MISMATCH);return;}
        result=WdfRequestRetrieveOutputBuffer(request,sizeof(CT_STATUS),(PVOID*)&status,NULL);
        if(!NT_SUCCESS(result)){WdfRequestComplete(request,result);return;}
        RtlZeroMemory(status,sizeof(*status));
    }else if(code==CT_IOCTL_READ){
        if(output!=sizeof(CT_BATCH)){WdfRequestComplete(request,STATUS_INFO_LENGTH_MISMATCH);return;}
        result=WdfRequestRetrieveOutputBuffer(request,sizeof(CT_BATCH),(PVOID*)&batch,NULL);
        if(!NT_SUCCESS(result)){WdfRequestComplete(request,result);return;}
        RtlZeroMemory(batch,sizeof(*batch));
    }else if(output!=0){WdfRequestComplete(request,STATUS_INFO_LENGTH_MISMATCH);return;}
    KeAcquireSpinLock(&c->Lock,&old);
    result=STATUS_SUCCESS;
    if(!c->Online)result=STATUS_DEVICE_NOT_READY;
    else if(owner==NULL||FileContext(owner)->Closing)result=STATUS_FILE_CLOSED;
    else if(code==CT_IOCTL_START){
        if(c->Active||(c->Owner&&c->Owner!=owner))result=STATUS_DEVICE_BUSY;
        else{c->Owner=owner;c->Active=TRUE;++c->Generation;c->Options=header->Options;
            c->TokenSeed=(CT_U64)KeQueryPerformanceCounter(NULL).QuadPart^KeQueryInterruptTime()^c->Generation;
            CtRingReset(&c->Index);
        }
    }else if(code==CT_IOCTL_STOP){
        if(c->Owner&&c->Owner!=owner)result=STATUS_ACCESS_DENIED;
        else{c->Active=FALSE;c->Owner=NULL;c->Options=0;c->Index.Head=c->Index.Count=0;}
    }else if(code==CT_IOCTL_STATUS&&status!=NULL){
        status->Magic=CT_MAGIC;status->Version=CT_VERSION;status->Size=sizeof(*status);status->Active=c->Active?1:0;
        status->QpcFrequency=c->Frequency;status->Produced=c->Index.Produced;status->Dropped=c->Index.Dropped;
        status->Available=c->Index.Count;status->Capacity=CT_RING_CAPACITY;status->Options=c->Options;bytes=sizeof(*status);
    }else if(code==CT_IOCTL_READ&&batch!=NULL){
        if(!c->Active||c->Owner!=owner)result=STATUS_ACCESS_DENIED;
        else{ULONG i;batch->Magic=CT_MAGIC;batch->Version=CT_VERSION;batch->Size=sizeof(*batch);batch->Dropped=c->Index.Dropped;
            batch->Count=min(c->Index.Count,CT_READ_CAPACITY);
            for(i=0;i<batch->Count;++i){batch->Events[i]=c->Ring[CtRingPop(&c->Index)];}
            bytes=sizeof(*batch);
        }
    }else result=STATUS_INVALID_DEVICE_REQUEST;
    KeReleaseSpinLock(&c->Lock,old);
    WdfRequestCompleteWithInformation(request,result,NT_SUCCESS(result)?bytes:0);
}
VOID DeviceCleanup(WDFOBJECT object){
    CT_DEVICE* c=Context(object);
    ULONG i;
    KIRQL old;
    KeAcquireSpinLock(&c->Lock,&old);c->Active=FALSE;c->Online=FALSE;KeReleaseSpinLock(&c->Lock,old);
    /* No flow contexts or asynchronous operations are ever created. The only
       registration/unregistration thread is the PnP lifetime thread. */
    for(i=0;i<6;++i)if(c->Registered[i]){
        NTSTATUS status;
        LARGE_INTEGER retry;
        retry.QuadPart=-100000; /* PASSIVE_LEVEL exceptional-path retry: 10 ms. */
        /* Never free a device while WFP still owns callback entry points. There
           are no flow contexts, and registration is not concurrent with cleanup;
           BUSY/IN_USE are therefore not normal outcomes. Unexpected subsystem
           failure retains the device rather than allowing a use-after-unload. */
        do{
            status=FwpsCalloutUnregisterById0(c->Callouts[i]);
            if(!NT_SUCCESS(status)&&status!=STATUS_FWP_CALLOUT_NOT_FOUND){
                (VOID)KeDelayExecutionThread(KernelMode,FALSE,&retry);
            }
        }while(!NT_SUCCESS(status)&&status!=STATUS_FWP_CALLOUT_NOT_FOUND);
        c->Registered[i]=FALSE;
    }
    ExWaitForRundownProtectionRelease(&c->Rundown);
    InterlockedCompareExchangePointer(&Singleton,NULL,(PVOID)object);
}
NTSTATUS DeviceAdd(WDFDRIVER driver,PWDFDEVICE_INIT init){
    WDFDEVICE device;
    WDF_OBJECT_ATTRIBUTES attributes;
    WDF_FILEOBJECT_CONFIG files;
    WDF_IO_QUEUE_CONFIG queue;
    WDFQUEUE q;
    CT_DEVICE* c;
    ULONG i;
    LARGE_INTEGER frequency;
    NTSTATUS status;
    DECLARE_CONST_UNICODE_STRING(sddl,L"D:P(A;;GA;;;SY)(A;;GA;;;BA)");
    UNREFERENCED_PARAMETER(driver);
    WdfDeviceInitSetDeviceType(init,CT_DEVICE_TYPE);
    WdfDeviceInitSetExclusive(init,TRUE);
    WdfDeviceInitSetCharacteristics(init,FILE_DEVICE_SECURE_OPEN|FILE_AUTOGENERATED_DEVICE_NAME,FALSE);
    status=WdfDeviceInitAssignSDDLString(init,&sddl);if(!NT_SUCCESS(status))return status;
    WDF_FILEOBJECT_CONFIG_INIT(&files,WDF_NO_EVENT_CALLBACK,WDF_NO_EVENT_CALLBACK,FileCleanup);
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributes,CT_FILE);attributes.ExecutionLevel=WdfExecutionLevelPassive;attributes.SynchronizationScope=WdfSynchronizationScopeNone;
    WdfDeviceInitSetFileObjectConfig(init,&files,&attributes);
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributes,CT_DEVICE);attributes.ExecutionLevel=WdfExecutionLevelPassive;attributes.SynchronizationScope=WdfSynchronizationScopeNone;attributes.EvtCleanupCallback=DeviceCleanup;
    status=WdfDeviceCreate(&init,&attributes,&device);if(!NT_SUCCESS(status))return status;
    c=Context(device);KeInitializeSpinLock(&c->Lock);ExInitializeRundownProtection(&c->Rundown);
    (VOID)KeQueryPerformanceCounter(&frequency);c->Frequency=(CT_U64)frequency.QuadPart;
    if(InterlockedCompareExchangePointer(&Singleton,device,NULL)!=NULL)return STATUS_OBJECT_NAME_COLLISION;
    status=WdfDeviceCreateDeviceInterface(device,&GUID_CT_INTERFACE,NULL);if(!NT_SUCCESS(status))return status;
    WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&queue,WdfIoQueueDispatchSequential);queue.EvtIoDeviceControl=DeviceControl;
    WDF_OBJECT_ATTRIBUTES_INIT(&attributes);attributes.ExecutionLevel=WdfExecutionLevelPassive;attributes.SynchronizationScope=WdfSynchronizationScopeNone;
    status=WdfIoQueueCreate(device,&queue,&attributes,&q);if(!NT_SUCCESS(status))return status;
    for(i=0;i<6;++i){
        FWPS_CALLOUT1 callout;RtlZeroMemory(&callout,sizeof(callout));callout.calloutKey=Keys[i];callout.classifyFn=Classify;callout.notifyFn=Notify;
        status=FwpsCalloutRegister1(WdfDeviceWdmGetDeviceObject(device),&callout,&c->Callouts[i]);if(!NT_SUCCESS(status))return status;
        c->Registered[i]=TRUE;
    }
    c->Online=TRUE;return STATUS_SUCCESS;
}
NTSTATUS DriverEntry(PDRIVER_OBJECT object,PUNICODE_STRING registry){
    WDF_DRIVER_CONFIG config;WDF_OBJECT_ATTRIBUTES attributes;
    WDF_DRIVER_CONFIG_INIT(&config,DeviceAdd);
    WDF_OBJECT_ATTRIBUTES_INIT(&attributes);attributes.ExecutionLevel=WdfExecutionLevelPassive;
    return WdfDriverCreate(object,registry,&attributes,&config,WDF_NO_HANDLE);
}
