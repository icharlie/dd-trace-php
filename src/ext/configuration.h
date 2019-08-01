#ifndef DD_CONFIGURATION_H
#define DD_CONFIGURATION_H
struct ddtrace_memoized_configuration_t;
extern struct ddtrace_memoized_configuration_t ddtrace_memoized_configuration;

void ddtrace_initialize_config();
void ddtrace_reload_config();

#define DD_CONFIGURATION                                                                                  \
    CHAR(get_dd_service_name, "DD_SERVICE_NAME", NULL, "Service name override") \
    CHAR(get_dd_trace_app_name, "DD_TRACE_APP_NAME", NULL, "Deprecated service name override") \
    CHAR(get_ddtrace_app_name, "DDTRACE_APP_NAME", NULL, "Deprecated service name override") \
    BOOL(get_dd_trace_cli_enabled, "DD_TRACE_CLI_ENABLED", "false", "Enable tracing in CLI") \
    BOOL(get_dd_trace_enabled, "DD_TRACE_ENABLED", "true", "Enable tracing globally") \
    CHAR(get_dd_agent_host, "DD_AGENT_HOST", "localhost", "")                                             \
    INT(get_dd_trace_agent_port, "DD_TRACE_AGENT_PORT", 8126, "")                                         \
    BOOL(get_dd_trace_agent_debug_verbose_curl, "DD_TRACE_AGENT_DEBUG_VERBOSE_CURL", FALSE, "")           \
    BOOL(get_dd_trace_debug_curl_output, "DD_TRACE_DEBUG_CURL_OUTPUT", FALSE, "")                         \
    CHAR(get_dd_trace_memory_limit, "DD_TRACE_MEMORY_LIMIT", NULL, "")                                    \
    INT(get_dd_trace_agent_flush_interval, "DD_TRACE_AGENT_FLUSH_INTERVAL", 5000, "")                     \
    INT(get_dd_trace_agent_flush_after_n_requests, "DD_TRACE_AGENT_FLUSH_AFTER_N_REQUESTS", 10, "")       \
    INT(get_dd_trace_agent_timeout, "DD_TRACE_AGENT_TIMEOUT", 500, "")                                    \
    INT(get_dd_trace_agent_connect_timeout, "DD_TRACE_AGENT_CONNECT_TIMEOUT", 100, "")                    \
    INT(get_dd_trace_debug_prng_seed, "DD_TRACE_DEBUG_PRNG_SEED", -1, "")                                 \
    BOOL(get_dd_log_backtrace, "DD_LOG_BACKTRACE", FALSE, "")                                             \
    INT(get_dd_trace_shutdown_timeout, "DD_TRACE_SHUTDOWN_TIMEOUT", 5000, "")                             \
    BOOL(get_dd_trace_send_traces_via_thread, "DD_TRACE_BETA_SEND_TRACES_VIA_THREAD", FALSE,              \
         "use background thread to send traces to the agent")                                             \
    INT(get_dd_trace_beta_high_memory_pressure_percent, "DD_TRACE_BETA_HIGH_MEMORY_PRESSURE_PERCENT", 80, \
        "reaching this percent threshold of a span buffer will trigger background thread "                \
        "to attempt to flush existing data to trace agent")
// render all configuration getters and define memoization struct
#include "configuration_render.h"

#endif  // DD_CONFIGURATION_H
