#include "malloc.h"


extern inline int MallocException_init (
    MallocException *e, const char *file, unsigned line, const char *func,
    char *what);


static int Exception_fputs_malloc (const BaseException *e_, FILE *stream) {
  const Exception *e = (const Exception *) e_;

  int ret = fputs("Fail to allocate memory", stream);
  if (e->what) {
    ret += fputs(" for ", stream);
    ret += fputs(e->what, stream);
  }
  ret += fputs(", possible out of memory\n", stream);
  return ret;
}


VTABLE_INIT(Exception, MallocException) = {
  .name = "MallocException",
  .destory = NULL,
  .fputs = Exception_fputs_malloc
};


extern inline void *malloc_e_snx (size_t size, char *file, unsigned line, const char *func, char *name);
