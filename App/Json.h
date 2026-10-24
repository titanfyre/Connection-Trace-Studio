#pragma once
#include "../SDK/ConnectionTrace.h"
#include <string>
namespace ConnectionTraceExport {
// JSON numbers lose uint64 precision in JavaScript. Keep opaque IDs as text.
inline std::string Endpoint(CT_U64 value){
 std::string text="\"0x0000000000000000\"";const char* hex="0123456789ABCDEF";
 for(unsigned i=0;i<16;++i){text[18-i]=hex[value&15];value>>=4;}
 return text;
}
}
