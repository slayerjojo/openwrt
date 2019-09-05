#ifndef __LOG_H__
#define __LOG_H__

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
    LOG_LEVEL_ERROR = 0,
    LOG_LEVEL_ACTION,
    LOG_LEVEL_DEBUG
};

#ifdef  _MSC_VER
#define __PRETTY_FUNCTION__ __FUNCTION__
#endif

#define LogError(...) log_println(__FILE__, __LINE__, __PRETTY_FUNCTION__, LOG_LEVEL_ERROR, __VA_ARGS__)
#define LogAction(...) log_println(__FILE__, __LINE__, __PRETTY_FUNCTION__, LOG_LEVEL_ACTION, __VA_ARGS__)
#define LogDump(level, prefix, buffer, size) log_dump(__FILE__, __LINE__, __PRETTY_FUNCTION__, (level), (prefix), (buffer), (size))

#define LogHalt(...) do {                                        \
    log_println(__FILE__, __LINE__, __PRETTY_FUNCTION__, 0, __VA_ARGS__);    \
    exit(EXIT_FAILURE);                                             \
} while(0);

#ifdef DEBUG
#define LogDebug(...) log_println(__FILE__, __LINE__, __PRETTY_FUNCTION__, 2, __VA_ARGS__)
#else
#define LogDebug(...)
#endif

void log_println(const char *file, size_t line, const char *func, unsigned char level, const char *fmt, ...);
void log_dump(const char *file, size_t line, const char *func, unsigned char level, const char *prefix, const void *buffer, size_t size);

#ifdef __cplusplus
}
#endif

#endif
