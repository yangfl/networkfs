#include <stdarg.h>
#include <stdio.h>

#include "log.h"


LOG_LEVEL log_level = LOG_LEVEL_WARN;


#if !(defined(LOGGING) && (LOGGING == 0))
void LOG_TRACE (LOG_LEVEL lvl, const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  vprintf(fmt, args);
  va_end(args);
}
#endif
