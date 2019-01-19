#ifndef NETWORKFS_LOG_H
#define NETWORKFS_LOG_H

#include <stdio.h>

#if 0
typedef enum {
  LOG_LEVEL_DEBUG = 0,
  LOG_LEVEL_INFO,
  LOG_LEVEL_WARN,
  LOG_LEVEL_ERROR
} LOG_LEVEL;

extern LOG_LEVEL log_level;

#if (defined(LOGGING) && (LOGGING == 0))
#define LOG_TRACE(...)
#else
void LOG_TRACE (LOG_LEVEL lvl, const char *fmt, ...);
#endif

#define LOG_ERROR(...) LOG_TRACE(LOG_LEVEL_ERROR, __VA_ARGS__)
#define LOG_WARN(...) LOG_TRACE(LOG_LEVEL_WARN, __VA_ARGS__)
#define LOG_INFO(...) LOG_TRACE(LOG_LEVEL_INFO, __VA_ARGS__)
#define LOG_DEBUG(...) LOG_TRACE(LOG_LEVEL_DEBUG, __VA_ARGS__)
#endif

#define eprintf(...) fprintf(stderr, __VA_ARGS__)
#define ERROR_LABEL "error: "
#define ERROR_HEADER eprintf("error at %s:%d: ", __FILE__, __LINE__);

#define dbg fprintf(stderr, "    run at %s: %d\n", __FILE__, __LINE__);

#endif /* NETWORKFS_LOG_H */
