#include "cervus.h"

class BaseServerContext {
    public:
        virtual void on_context_init(Resource ctx) {

        }

        virtual void before_request(BasicRequestInfo *info) {

        }
};

void _do_context_init(BaseServerContext **indirect, Resource ctx);

static void _on_context_init(Memory mem, Resource ctx) {
    BaseServerContext **indirect = (BaseServerContext **) mem;
    _do_context_init(indirect, ctx);
}

static void _before_request(Memory mem, BasicRequestInfo *info) {
    BaseServerContext *s = * (BaseServerContext **) mem;
    s -> before_request(info);
}

extern "C" void cervus_module_init(ModuleInitConfig *cfg) {
    cfg -> server_context_mem_size = sizeof(BaseServerContext *);
    cfg -> context_init_hook = _on_context_init;
    cfg -> before_request_hook = _before_request;
    cfg -> ok = 1;
}
