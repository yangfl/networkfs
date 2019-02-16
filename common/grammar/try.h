#ifndef TRY_H
#define TRY_H

#include "macro.h"
#include "with.h"
#include "exception.h"


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


#endif /* TRY_H */
