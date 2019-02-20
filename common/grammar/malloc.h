#ifndef MALLOCE_H
#define MALLOCE_H

#include <stdlib.h>

#include "try.h"


typedef Exception MallocException;
VTABLE_IMPL(Exception, MallocException);

inline int MallocException_init (
    MallocException *e, const char *file, const char *func, unsigned line,
    char *what) {
  e->what = what;
  e->VTABLE(Exception) = &VTABLE_OF(Exception, MallocException);

  return Exception_init((Exception *) e, file, func, line, 0);
}

#define MallocException(msg) GENERATE_EXCEPTION_INIT(MallocException, msg)

inline void *malloc_e_snx (size_t size, const char *file, const char *func, unsigned line, char *name) {
  void *ret = malloc(size);
  if unlikely (ret == NULL) {
    MallocException_init(&ex, file, func, line, name);
  }
  return ret;
}

#define malloc_e_sn(size, name) malloc_e_snx(size, __FILE__, __func__, __LINE__, name)
#define malloc_e_s(size) malloc_e_snx(size, __FILE__, __func__, __LINE__, NULL)
#define malloc_e_generic(...) GET_3RD_ARG(__VA_ARGS__, malloc_e_sn, malloc_e_s, )
#define malloc_e(...) malloc_e_generic(__VA_ARGS__)(__VA_ARGS__)

#define malloc_et(type) ((type *) malloc_e_sn(sizeof(type), # type))
#define malloc_t(type) ((type *) malloc(sizeof(type)))

#define erase(p) (free(p), (p) = NULL)


#endif /* MALLOCE_H */
