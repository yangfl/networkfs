#ifndef _CLASS_H
#define _CLASS_H

#include "macro.h"
#include "with.h"
#include "vtable.h"


/*
CLASS(A, {
  INHERIT(B);
}) a;
CAST(B, a);
*/
#define CLASS(Class, ...) \
  typedef struct VTABLE_CLASSNAME(Class) VTABLE_CLASSNAME(Class); \
  typedef struct Class __VA_ARGS__ Class; \
  struct Class
#define SUPCLASS_FIELD(SupClass) __sup_ ## SupClass
#define INHERIT(SupClass) \
  union { \
    struct SupClass; \
    struct SupClass SUPCLASS_FIELD(SupClass); \
  }
#define CAST(SupClass, instance) (instance).SUPCLASS_FIELD(SupClass)

#define new(type) CONCAT(type, _new)
#define __generate_catch_value(decl, lvar, exec, counter) \
  for (decl;;) if (1) { \
    goto CONCAT(__catch_value_start_, counter); \
    CONCAT(__catch_value_end_, counter): \
    exec; break; \
  } else while (1) if (1) goto CONCAT(__catch_value_end_, counter); \
    else CONCAT(__catch_value_start_, counter): lvar
#define catch_value(...) __generate_catch_value(__VA_ARGS__, __COUNTER__)
#define delete(type) catch_value( \
  void *__delete_ptr, __delete_ptr =, type ## _free(__delete_ptr))

#define scope(type, varname, ...) with (auto varname = new(type) (__VA_ARGS__), delete(type) varname)


#endif /* _CLASS_H */
