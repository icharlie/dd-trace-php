#include "execute_hooks.h"

#include <php.h>
// the space below prevents clang-format from re-ordering the headers, which breaks compilation

#include <Zend/zend_exceptions.h>
#include <Zend/zend_execute.h>
#include <signal.h>

#include "ddtrace.h"
#include "dispatch_compat.h"
#include "env_config.h"
#include "logging.h"
#include "trace.h"

#if PHP_VERSION_ID < 50500
ZEND_EXTERN_MODULE_GLOBALS(ddtrace);

static ddtrace_dispatch_t *_find_dispatch(zval *this, zend_op_array *op_array TSRMLS_DC) {
    zend_class_entry *class = NULL;
    zval fname;
    ddtrace_dispatch_t *dispatch;

    INIT_ZVAL(fname);
    ZVAL_STRING(&fname, op_array->function_name, 0);

    class = this ? Z_OBJCE_P(this) : op_array->scope;

    dispatch = class ? find_method_dispatch(class, &fname TSRMLS_CC)
                     : find_function_dispatch(DDTRACE_G(function_lookup), &fname);

    return dispatch;
}

BOOL_T _should_trace(zend_execute_data *execute_data, zend_function **fbc, ddtrace_dispatch_t **dispatch TSRMLS_DC) {
    if (DDTRACE_G(disable) || DDTRACE_G(disable_in_current_request) || DDTRACE_G(class_lookup) == NULL ||
        DDTRACE_G(function_lookup) == NULL) {
        return FALSE;
    }

    if (!execute_data) {
        return FALSE;
    }

    *fbc = execute_data->function_state.function;
    if (!*fbc) {
        return FALSE;
    }

    // Don't trace closures; are there such things as internal closures?
    if ((*fbc)->common.fn_flags & ZEND_ACC_CLOSURE) {
        return FALSE;
    }

    if (!(*fbc)->common.function_name) {
        return FALSE;
    }

    zval fname;
    INIT_ZVAL(fname);
    ZVAL_STRING(&fname, (*fbc)->common.function_name, 0);

    /* this is a stripped down ddtrace_this; maybe refactor? */
#if PHP_VERSION_ID < 50500
    zval *this = execute_data->object ? execute_data->object : NULL;
#else
    zval *this = execute_data->call ? execute_data->call->object : NULL;
#endif

    if (this && Z_TYPE_P(this) != IS_OBJECT) {
        this = NULL;
    }

    *dispatch = ddtrace_find_dispatch(this, *fbc, &fname TSRMLS_CC);

    if (!*dispatch || (*dispatch)->busy) {
        return FALSE;
    }

    return TRUE;
}

// todo: re-evaluate name
BOOL_T _should_trace_user(zend_op_array *op_array, ddtrace_dispatch_t **dispatch TSRMLS_DC) {
    if (DDTRACE_G(disable) || DDTRACE_G(disable_in_current_request) || DDTRACE_G(class_lookup) == NULL ||
        DDTRACE_G(function_lookup) == NULL) {
        return FALSE;
    }

    if (!op_array->function_name) {
        return FALSE;
    }

    if (op_array->fn_flags & ZEND_ACC_CLOSURE) {
        return FALSE;
    }

    *dispatch = _find_dispatch(EG(This), op_array TSRMLS_CC);
    if (!*dispatch || (*dispatch)->busy) {
        return FALSE;
    }

    if (!(*dispatch)->run_as_postprocess) {
        return FALSE;
    }

    return TRUE;
}

// true globals; should only be modified during minit/mshutdown
static void (*_execute)(zend_op_array *op_array TSRMLS_DC);
static void _ddtrace_execute(zend_op_array *op_array TSRMLS_DC) {
    ddtrace_dispatch_t *dispatch;

    if (!_should_trace_user(op_array, &dispatch)) {
        _execute(op_array TSRMLS_CC);
        return;
    }

    ddtrace_class_lookup_acquire(dispatch);
    dispatch->busy = 1;

    ddtrace_span_t *span = ddtrace_open_span(TSRMLS_C);

    _execute(op_array TSRMLS_CC);
    dd_trace_stop_span_time(span);

    if (!EG(exception) && Z_TYPE(dispatch->callable) == IS_OBJECT) {
        zval *rv_ptr = EG(return_value_ptr_ptr) ? *EG(return_value_ptr_ptr) : EG(uninitialized_zval_ptr);
        int orig_error_reporting = EG(error_reporting);
        EG(error_reporting) = 0;

        ddtrace_execute_tracing_closure(&dispatch->callable, span->span_data, EG(current_execute_data),
                                        rv_ptr TSRMLS_CC);
        EG(error_reporting) = orig_error_reporting;
        // If the tracing closure threw an exception, ignore it to not impact the original call
        if (EG(exception)) {
            ddtrace_log_debug("Exeception thrown in the tracing closure");
            zend_clear_exception(TSRMLS_C);
        }
    }

    ddtrace_close_span(TSRMLS_C);

    dispatch->busy = 0;
    ddtrace_class_lookup_release(dispatch);
}

static void (*_prev_execute_internal)(zend_execute_data *, int return_value_used TSRMLS_DC);
static void (*_execute_internal)(zend_execute_data *, int return_value_used TSRMLS_DC);

void _ddtrace_execute_internal(zend_execute_data *execute_data, int return_value_used TSRMLS_DC) {
    ddtrace_dispatch_t *dispatch;
    zend_function *fbc;

    if (!_should_trace(execute_data, &fbc, &dispatch)) {
        _execute_internal(execute_data, return_value_used TSRMLS_CC);
        return;
    }

    ddtrace_class_lookup_acquire(dispatch);
    dispatch->busy = 1;

    ddtrace_span_t *span = ddtrace_open_span(TSRMLS_C);

#if PHP_VERSION_ID < 50400
    // This *might* work on PHP 5.3:
    zval **rv_ptr = &(*(temp_variable *)((char *)execute_data->Ts + execute_data->opline->result.u.var)).var.ptr;
#else
    // this line was lifted from execute_internal
    zval **rv_ptr = &(*(temp_variable *)((char *)execute_data->Ts + execute_data->opline->result.var)).var.ptr;
#endif

    _execute_internal(execute_data, return_value_used TSRMLS_CC);
    dd_trace_stop_span_time(span);

    if (!EG(exception) && Z_TYPE(dispatch->callable) == IS_OBJECT) {
        int orig_error_reporting = EG(error_reporting);
        EG(error_reporting) = 0;

        ddtrace_execute_tracing_closure(&dispatch->callable, span->span_data, execute_data, *rv_ptr TSRMLS_CC);
        EG(error_reporting) = orig_error_reporting;
        // If the tracing closure threw an exception, ignore it to not impact the original call
        if (EG(exception)) {
            // TODO Log the exception
            zend_clear_exception(TSRMLS_C);
        }
    }

    ddtrace_close_span(TSRMLS_C);

    dispatch->busy = 0;
    ddtrace_class_lookup_release(dispatch);
}

static stack_t ss;
static struct sigaction sa;
static void _sigsegv_handler(int sig) {
    perror("Segmentation fault. Try raising stack size (see ulimit).\n");
    exit(EXIT_FAILURE);
}

void ddtrace_execute_hooks_init(void) {
    _execute = zend_execute;
    zend_execute = _ddtrace_execute;

    _execute_internal = zend_execute_internal ? zend_execute_internal : execute_internal;
    _prev_execute_internal = zend_execute_internal;
    zend_execute_internal = _ddtrace_execute_internal;

    /* Since we use the zend_execute hook, the VM behavior changes and we can
     * run out of stack memory and get a segmentation fault.
     * This code installs a new stack that the OS will use to execute signals.
     */
    ss.ss_sp = malloc(SIGSTKSZ);
    if (ss.ss_sp == NULL) {
        perror("malloc failed");
        exit(EXIT_FAILURE);
    }

    ss.ss_size = SIGSTKSZ;
    ss.ss_flags = 0;
    if (sigaltstack(&ss, NULL) == -1) {
        perror("sigaltstack failed");
        exit(EXIT_FAILURE);
    }

    sa.sa_flags = SA_ONSTACK;
    sa.sa_handler = _sigsegv_handler; /* Address of a signal handler */
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGSEGV, &sa, NULL) == -1) {
        perror("sigaction failed to register SIGSEGV");
        exit(EXIT_FAILURE);
    }
}

void ddtrace_execute_hooks_shutdown(void) {
    zend_execute = _execute;
    zend_execute_internal = _prev_execute_internal;

    if (ss.ss_sp) {
        free(ss.ss_sp);
    }
}
#else

void ddtrace_execute_hooks_init(void) {}
void ddtrace_execute_hooks_shutdown(void) {}

#endif
