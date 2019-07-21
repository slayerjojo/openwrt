#include "log.h"
#include <stdio.h>
#include <stdarg.h>
#include <time.h>

inline void log_trace(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
    
    fflush(stdout);
}

inline void log_println(const char *file, size_t line, const char *func, unsigned char level, const char *fmt, ...)
{
    static size_t seq = 0;

    printf("\n%ld|%s:%4ld|%s|lv:%d|", 
        seq++, 
        file, 
        line,
        func,
        level);

    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);

    fflush(stdout);
}
