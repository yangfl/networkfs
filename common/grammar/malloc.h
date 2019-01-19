#ifndef MALLOCE_H
#define MALLOCE_H

#include <stdlib.h>

#include "try.h"


typedef Exception MallocException;
VTABLE_IMPL(Exception, MallocException);

int MallocException_init (
    MallocException *e, const char *file, unsigned line, const char *func,
    char *what);

#define MallocException(msg) GENERATE_EXCEPTION_INIT(MallocException, msg)

inline void *malloc_e_snx (size_t size, char *file, unsigned line, const char *func, char *name) {
  void *ret = malloc(size);
  if unlikely (ret == NULL) {
    MallocException_init(&ex, file, line, func, name);
  }
  return ret;
}

#define malloc_e_sn(size, name) malloc_e_snx(size, __FILE__, __LINE__, __func__, name)
#define malloc_e_s(size) malloc_e_snx(size, __FILE__, __LINE__, __func__, NULL)
#define malloc_e_generic(...) GET_3RD_ARG(__VA_ARGS__, malloc_e_sn, malloc_e_s, )
#define malloc_e(...) malloc_e_generic(__VA_ARGS__)(__VA_ARGS__)

#define malloc_et(type) ((type *) malloc_e_sn(sizeof(type), # type))

#define erase(p) (free(p), (p) = NULL)


#endif /* MALLOCE_H */
