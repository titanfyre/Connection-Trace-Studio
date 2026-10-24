#ifndef TITAN_CONNECTION_TRACE_H
#define TITAN_CONNECTION_TRACE_H
/* Public, pointer-free wire protocol. Kernel implementation is private. */
#define CT_VERSION 1u
#define CT_DEVICE_TYPE 0x8337u
#define CT_MAGIC 0x43545243u
#define CT_RING_CAPACITY 2048u
#define CT_READ_CAPACITY 64u
#define CT_PATH_CAPACITY 128u
#define CT_STAGE_AUTHORIZE 1u
#define CT_STAGE_ESTABLISHED 2u
#define CT_STAGE_CLOSED 3u
#define CT_FLAG_PID 1u
#define CT_FLAG_ENDPOINT 2u
#define CT_FLAG_PATH 4u
#define CT_FLAG_PATH_TRUNCATED 8u
#define CT_FLAG_REAUTHORIZE 16u
#define CT_FLAG_LOCAL_ADDRESS 32u
#define CT_FLAG_REMOTE_ADDRESS 64u
#define CT_FLAG_PORTS 128u
#define CT_FLAG_PROTOCOL 256u
#define CT_FLAGS_ALL 511u
#define CT_OPTION_INCLUDE_PATHS 1u
#define CT_CONTROL_START 1u
#define CT_CONTROL_STOP 2u
#define CT_CONTROL_STATUS 3u
#define CT_CONTROL_READ 4u
#define CT_IOCTL(function) ((CT_DEVICE_TYPE<<16)|(3u<<14)|((0x800u+(function))<<2))
#define CT_IOCTL_START CT_IOCTL(CT_CONTROL_START)
#define CT_IOCTL_STOP CT_IOCTL(CT_CONTROL_STOP)
#define CT_IOCTL_STATUS CT_IOCTL(CT_CONTROL_STATUS)
#define CT_IOCTL_READ CT_IOCTL(CT_CONTROL_READ)
typedef unsigned char CT_U8;
typedef unsigned short CT_U16;
typedef unsigned int CT_U32;
typedef unsigned long long CT_U64;
typedef struct CT_REQUEST { CT_U32 Magic,Version,Size,Options; } CT_REQUEST;
typedef struct CT_EVENT {
    CT_U64 Sequence,TimestampQpc,ProcessId,EndpointId;
    CT_U32 Flags;
    CT_U16 LocalPort,RemotePort;
    CT_U8 Family,Stage,Protocol,Reserved;
    CT_U8 LocalAddress[16],RemoteAddress[16];
    CT_U16 PathLength,Reserved2;
    CT_U16 Application[CT_PATH_CAPACITY];
} CT_EVENT;
typedef struct CT_STATUS {
    CT_U32 Magic,Version,Size,Active;
    CT_U64 QpcFrequency,Produced,Dropped;
    CT_U32 Available,Capacity,Options,Reserved;
} CT_STATUS;
typedef struct CT_BATCH {
    CT_U32 Magic,Version,Size,Count;
    CT_U64 Dropped;
    CT_EVENT Events[CT_READ_CAPACITY];
} CT_BATCH;

#ifdef __cplusplus
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
namespace ConnectionTrace {
static_assert(sizeof(CT_REQUEST)==16 && sizeof(CT_EVENT)==336 && sizeof(CT_STATUS)==56,
              "Unexpected compiler ABI; use the default MSVC packing.");
bool Validate(const CT_STATUS&) noexcept;
bool Validate(const CT_BATCH&,std::size_t bytes) noexcept;
std::wstring Address(const CT_EVENT&,bool remote);
std::wstring Application(const CT_EVENT&);
const wchar_t* Stage(const CT_EVENT&) noexcept;
struct Lifetime {
    CT_U64 endpoint=0,pid=0,authorize=0,established=0,closed=0;
    bool missing_start=false;
    bool pid_known=false;
    bool incomplete=false;
};
/* Bounded, best-effort correlation. Dropped events make completeness unknown. */
class Correlator {
    std::vector<Lifetime> entries_;
    CT_U64 evicted_=0;
public:
    const Lifetime* Observe(const CT_EVENT&);
    const std::vector<Lifetime>& Entries() const noexcept{return entries_;}
    CT_U64 Evicted() const noexcept{return evicted_;}
};
class Session {
    struct State;
    std::unique_ptr<State> state_;
public:
    Session();
    ~Session();
    Session(const Session&)=delete;
    Session& operator=(const Session&)=delete;
    void Start(bool includePaths=false); // Paths are opt-in; administrator only.
    void Stop() noexcept;         // Removes only this session's WFP objects.
    CT_STATUS Status() const;
    std::vector<CT_EVENT> Read(); // Nonblocking; at most 64 events.
    static std::string Error(unsigned long code);
};
}
#endif
#endif
