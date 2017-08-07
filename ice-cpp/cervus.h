#ifndef _CERVUS_H_
#define _CERVUS_H_

#include "imports.h"

struct BasicRequestInfo {
    const char *uri;
    const char *remote_addr;
    const char *method;
    Resource response;
};

typedef void (*ContextInitHookFn)(Memory mem, Resource ctx);
typedef void (*ContextDestroyHookFn)(Memory mem, Resource ctx);
typedef void (*BeforeRequestHookFn)(Memory mem, struct BasicRequestInfo *info);
typedef void (*RequestHookFn)(Memory mem, Resource req);
typedef void (*ResponseHookFn)(Memory mem, Resource resp);

struct ModuleInitConfig {
    char ok;
    unsigned int server_context_mem_size;
    ContextInitHookFn context_init_hook;
    ContextDestroyHookFn context_destroy_hook;
    BeforeRequestHookFn before_request_hook;
    RequestHookFn request_hook;
    ResponseHookFn response_hook;
};

#endif
