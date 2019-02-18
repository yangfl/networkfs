#ifndef VTABLE_H
#define VTABLE_H

#include <stdbool.h>
#include <stddef.h>

#include <crc32.h>

#include "macro.h"
#include "with.h"


extern unsigned int VTable_rand;

typedef struct VTable {
  unsigned int __vtable_checksum;
} VTable;

#define VTABLE_CLASSNAME(Class) CONCAT(Class, __VTable)
#define VTABLE_FIELDNAME(Class) CONCAT(__vtable_, Class)
#define VTABLE_DECL(Class, ...) \
  const struct VTABLE_CLASSNAME(Class) { \
    struct VTable; \
    const struct __VA_ARGS__; \
  } *VTABLE_FIELDNAME(Class)
#define VTABLE(...) IIF(COMPARE(x,x GET_2ND_ARG(__VA_ARGS__,)))(VTABLE_FIELDNAME, VTABLE_DECL)(__VA_ARGS__)
#define VTABLE_OF(SupClass, SubClass) CONCAT(CONCAT(SupClass, _vtable_), SubClass)
#define VTABLE_IMPL(SupClass, SubClass) \
  extern struct VTABLE_CLASSNAME(SupClass) VTABLE_OF(SupClass, SubClass)
#define VTABLE_INIT(SupClass, SubClass) \
  VTABLE_IMPL(SupClass, SubClass); \
  \
  static void __attribute__((constructor)) CONCAT(VTABLE_OF(SupClass, SubClass), _load) (void) { \
    VTable_init((struct VTable *) &VTABLE_OF(SupClass, SubClass), sizeof(VTABLE_OF(SupClass, SubClass))); \
  } \
  \
  struct VTABLE_CLASSNAME(SupClass) VTABLE_OF(SupClass, SubClass)
#define virtual_method(SupClass, instance, method) \
  !likely( \
      (instance)->VTABLE_FIELDNAME(SupClass) && \
      (instance)->VTABLE_FIELDNAME(SupClass)->method && \
      VTable_vaild( \
        (const struct VTable *) (instance)->VTABLE_FIELDNAME(SupClass), \
        sizeof(*((instance)->VTABLE_FIELDNAME(SupClass))))) ? \
    0 : ((instance)->VTABLE_FIELDNAME(SupClass)->method)
#define issubtype(SupClass, instance, SubClass) \
  ((instance)->VTABLE_FIELDNAME(SupClass) == &VTABLE_OF(SupClass, SubClass))

void VTable_rand_init (void);
int __attribute__((noreturn)) VTable_crash (void);

inline unsigned int VTable_checksum (const struct VTable *this, unsigned int size) {
  return crc32(
    ((const unsigned char *) this) + sizeof(struct VTable),
    size - sizeof(struct VTable)
  ) ^ VTable_rand;
}

inline bool VTable_vaild (const struct VTable *this, unsigned int size) {
  if (this == NULL) {
    return false;
  }
  if (this->__vtable_checksum != VTable_checksum(this, size)) {
    VTable_crash();
  }
  return true;
}

inline int VTable_init (struct VTable *this, unsigned int size) {
  VTable_rand_init();
  this->__vtable_checksum = VTable_checksum(this, size);
  return 0;
}


#endif /* VTABLE_H */
