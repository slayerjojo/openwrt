#include "log.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>

inline void log_println(const char *file, size_t line, const char *func, unsigned char level, const char *fmt, ...)
{
    time_t now  = time(0);
    struct tm *t = localtime(&now);

    static size_t seq = 0;

    printf("\n\033[37m%02d:%02d:%02d|\n%ld|%s:%4ld|%s|\033[%dmlv:%d|", 
        t->tm_hour, 
        t->tm_min, 
        t->tm_sec, 
        seq++, 
        file, 
        line,
        func,
        (!level ? 31 : 37),
        level);

    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);

    fflush(stdout);
}

void log_dump(const char *file, size_t line, const char *func, unsigned char level, const char *prefix, const void *buffer, size_t size)
{
    size_t i = 0;
    time_t now = time(0);
    struct tm *t = localtime(&now);

    static size_t seq = 0;

    printf("\n\033[37m%02d:%02d:%02d|%ld|%s:%ld|%s|\033[%dmlv:%d|%s ", 
        t->tm_hour, 
        t->tm_min, 
        t->tm_sec, 
        seq++, 
        file, 
        line,
        func,
        (!level ? 31 : 37),
        level,
        prefix);

    for (i = 0; i < size; i++)
        printf("%02x", *(((unsigned char *)buffer) + i));

    fflush(stdout);
}
