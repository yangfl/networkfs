#ifndef TRY_H
#define TRY_H

#include <threads.h>
#include <stdbool.h>
#include <stdio.h>

#include "macro.h"
#include "with.h"
#include "_class.h"


#define BT_BUF_SIZE 32

typedef struct BaseException BaseException;

#define EXCEPTION_CATCHED 2

typedef int (*Exception_fputs_t) (const BaseException *, FILE *);
typedef void (*Exception_del_t) (BaseException *);


CLASS(BaseException, {
  char set;
  int nptrs;
  unsigned line;
  const char *file;
  const char *func;
  void *bt[BT_BUF_SIZE];
  VTABLE(Exception, {
    const char *name;
    Exception_fputs_t fputs;
    Exception_del_t clean;
  });
});

CLASS(Exception, {
  INHERIT(BaseException);
  char *what;
  char padding[1024 - sizeof(struct BaseException) - sizeof(char *)];
});

#define ExceptionDef(name, ...) \
  typedef struct name { \
    struct BaseException; \
    struct __VA_ARGS__; \
  } name; \
  _Static_assert(sizeof(name) <= sizeof(Exception), # name " too large"); \
  VTABLE_IMPL(Exception, name)

extern thread_local Exception ex;

#define GENERATE_EXCEPTION_INIT(type, ...) type ## _init((type *) &ex, __FILE__, __LINE__, __func__, ## __VA_ARGS__)

void Exception_copy (Exception *dst, const Exception *src);
int Exception_fputs (const Exception *e, FILE *stream);

inline bool Exception_has (const Exception *e) {
  return e->set == true;
}

void Exception_dirtydestory (Exception *e);
void Exception_destory (Exception *e);
int Exception_init (Exception *e, const char *file, unsigned line, const char *func, bool no_bt);


#define should(test) if likely (test)
#define otherwise ; else
#define check if unlikely (Exception_has(&ex)) break
#define onsuccess if likely (!Exception_has(&ex))

#define __generate_try(counter) \
  if (1) goto CONCAT(__try_start_, counter); \
  else CONCAT(__try_end_, counter): if likely (!Exception_has(&ex)); \
  else if (0) while (1) if (1) goto CONCAT(__try_end_, counter); \
    else CONCAT(__try_start_, counter): do_once
#define try __generate_try(__COUNTER__)
#define catch_type(type, e) else if (issubtype(Exception, &ex, type)) with ( \
  type unused *e = (type *) &ex, e->set = EXCEPTION_CATCHED, 1, if (e->set != true) Exception_destory((Exception *) e))
#define catch_default(e) else with ( \
  Exception unused *e = &ex, e->set = EXCEPTION_CATCHED, 1, if (e->set != true) Exception_destory(e))
#define catch_generic(...) GET_3RD_ARG(__VA_ARGS__, catch_type, catch_default, )
#define catch(...) catch_generic(__VA_ARGS__)(__VA_ARGS__)
#define onerror_type(type, e) else if (issubtype(Exception, &ex, type)) with (type unused *e = (type *) &ex,, 1,)
#define onerror_default(e) else with (Exception unused *e = &ex,, 1,)
#define onerror_generic(...) GET_3RD_ARG(__VA_ARGS__, onerror_type, onerror_default, )
#define onerror(...) onerror_generic(__VA_ARGS__)(__VA_ARGS__)

#define throwable do_after(check)
#define throw reflow(, 1, ex.set = true; break)
#define condition_throw(test) if likely (test); else throw

#define hideE with ( \
  Exception __save_ex, \
  Exception_copy(&__save_ex, &ex); ex.set = false, \
  1, Exception_copy(&ex, &__save_ex))

#define TEST_SUCCESS Exception_has(&ex)

#define CONSTSTR(s) s, strlen(s)


/* unspecified */

typedef Exception UnspecifiedException;
VTABLE_IMPL(Exception, UnspecifiedException);

int UnspecifiedException_init (
    UnspecifiedException *e, const char *file, unsigned line, const char *func,
    char *what);

#define UnspecifiedException(msg) GENERATE_EXCEPTION_INIT(UnspecifiedException, strdup(msg))
#define Quick Exception(UnspecifiedException, "Quick exception")


#endif /* TRY_H */
