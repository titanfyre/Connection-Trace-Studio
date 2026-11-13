#include "ConnectionTraceStudioJsonSDK.h"
#include <string.h>
int main(void){uint64_t session=0;if(connection_trace_studio_json_open(&session))return 1;char *reply=connection_trace_studio_json_call(session,"{",1);if(!reply){connection_trace_studio_json_close(session);return 2;}int failed=strstr(reply,"\"ok\":false")==0;connection_trace_studio_json_free(reply);if(connection_trace_studio_json_close(session))return 3;return failed;}
