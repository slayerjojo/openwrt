#ifndef __LOG_H__
#define __LOG_H__

#include <stddef.h>

#ifdef  _MSC_VER
#define __PRETTY_FUNCTION__ __FUNCTION__
#endif

#ifdef MICROCHIP

#define LogError(...)
#define LogAction(...)
#define LogTrace(...)
#define LogHalt(...) while(1);

#else

#define LogError(...) log_println(__FILE__, __LINE__, __PRETTY_FUNCTION__, 0, __VA_ARGS__)
#define LogAction(...) log_println(__FILE__, __LINE__, __PRETTY_FUNCTION__, 1, __VA_ARGS__)
#define LogTrace(...) log_trace(__VA_ARGS__)

#define LogHalt(...) do {                                        \
    log_println(__FILE__, __LINE__, __PRETTY_FUNCTION__, 0, __VA_ARGS__);    \
    exit(EXIT_FAILURE);                                             \
} while(0);

#ifdef DEBUG
#define LogDebug(...) log_println(__FILE__, __LINE__, __PRETTY_FUNCTION__, 2, __VA_ARGS__)
#define LogDebugTrace(...) log_trace(__VA_ARGS__)
#else
#define LogDebug(...)
#define LogDebugTrace(...)
#endif

void log_trace(const char *fmt, ...);
void log_println(const char *file, size_t line, const char *func, unsigned char level, const char *fmt, ...);

#endif

#endif
