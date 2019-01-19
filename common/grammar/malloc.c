#include "malloc.h"


int MallocException_init (
    MallocException *e, const char *file, unsigned line, const char *func,
    char *what) {
  e->what = what;
  e->VTABLE(Exception) = &VTABLE_OF(Exception, MallocException);

  return Exception_init((Exception *) e, file, line, func, 0);
}


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


static void Exception_del_malloc (BaseException *e_) {
  Exception *e = (Exception *) e_;

  e->what = NULL;
}


VTABLE_INIT(Exception, MallocException) = {
  .name = "MallocException",
  .clean = Exception_del_malloc,
  .fputs = Exception_fputs_malloc
};


extern inline void *malloc_e_snx (size_t size, char *file, unsigned line, const char *func, char *name);
