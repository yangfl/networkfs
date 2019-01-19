#ifndef WITH_H
#define WITH_H

#include "macro.h"


#define __generate_do_before(expr, counter) \
  if (1) { expr; goto CONCAT(__do_before_, counter);  } else CONCAT(__do_before_, counter):
#define do_before(expr) __generate_do_before(expr, __COUNTER__)
#define __generate_reflow(init, condition, cleanup, counter) \
  if (1) { \
    init; \
    if (condition) goto CONCAT(__reflow_start_, counter); \
    CONCAT(__reflow_end_, counter): cleanup; \
  } else while (1) if (1) goto CONCAT(__reflow_end_, counter); \
    else CONCAT(__reflow_start_, counter): do_once
#define reflow(init, condition, cleanup) __generate_reflow(init, condition, cleanup, __COUNTER__)
#define do_after(expr) reflow(, 1, expr)
#define break_after do_after(break)

#define let(decl) for (decl;;) reflow(, 1, break)

#define with_test(decl, init, condition, cleanup) \
  for (decl;;) reflow(init, condition, cleanup; break)
#define with_seperated(decl, init, cleanup) \
  with_test(decl, init, likely (!Exception_has(&ex)), cleanup)
#define with_assign(decl, cleanup) \
  with_test(decl,     , likely (!Exception_has(&ex)), cleanup)
#define with_decl(decl) \
  with_test(decl,     , likely (!Exception_has(&ex)),        )
#define with_generic(...) GET_5TH_ARG(__VA_ARGS__, with_test, with_seperated, with_assign, with_decl, )
#define with(...) with_generic(__VA_ARGS__)(__VA_ARGS__)

#define do_once switch (1) case 1:


#endif /* WITH_H */
