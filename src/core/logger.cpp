#include "logger.h"

#include <cstdio>
#include <cstring>
#include <cstdarg>

bool logger_init() {
    return true;
}

void logger_quit() {

}

void logger_output(LogLevel log_level, const char* message, ...) {
    const char* LOG_PREFIX[4] = { "ERROR", "WARN", "INFO", "DEBUG" };
    const size_t MESSAGE_BUFFER_LENGTH = 32000;
    char out_message[MESSAGE_BUFFER_LENGTH];

    memset(out_message, 0, sizeof(out_message));

    __builtin_va_list arg_ptr;
    va_start(arg_ptr, message);
    vsnprintf(out_message, MESSAGE_BUFFER_LENGTH, message, arg_ptr);
    va_end(arg_ptr);

    printf("[%s]: %s\n", LOG_PREFIX[log_level], out_message);
}
