#include "handlers_internal.h"

void ddtrace_couchbase_handlers_startup(void) {
    // clang-format off
    ddtrace_string methods[] = {
        DDTRACE_STRING_LITERAL("remove"),
        DDTRACE_STRING_LITERAL("insert"),
        DDTRACE_STRING_LITERAL("upsert"),
        DDTRACE_STRING_LITERAL("replace"),
        DDTRACE_STRING_LITERAL("get"),
        DDTRACE_STRING_LITERAL("query"),
    };
    // clang-format on

    ddtrace_string couchbase_bucket = DDTRACE_STRING_LITERAL("couchbase\\bucket");
    size_t methods_len = sizeof methods / sizeof methods[0];
    ddtrace_replace_internal_methods(couchbase_bucket, methods_len, methods);
}
