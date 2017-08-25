#ifndef _CERVUS_H_
#define _CERVUS_H_

#include "imports.h"

struct BasicRequestInfo {
    const char *uri;
    const char *remote_addr;
    const char *method;
    Resource response;
    Resource custom_properties;
};

typedef void (*HookFn)(Resource hook_context);

#ifdef __cplusplus
extern "C" {
#endif

void cervus_log(int level, const char *msg);
void add_hook(const char *name, HookFn cb);
Resource downcast_hook_context(Resource hook_context, const char *target_type);
void * reset_module_mem(unsigned int size);
Resource get_module_mem();
#ifdef __cplusplus
}
#endif

#endif
