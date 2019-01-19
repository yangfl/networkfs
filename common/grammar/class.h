#ifndef CLASS_H
#define CLASS_H

#include "malloc.h"
#include "_class.h"


#define PROTECT_RETURN(this) if unlikely ((this) == NULL) return
#define GENERATE_NEW_FUNC(Class, Struct, variant, args, ...)  \
  static inline Struct *Class ## _new_ ## variant args { \
    Struct *ret = malloc_et(Struct); \
    if likely (ret) { \
      Class ## _init_ ## variant(ret, ## __VA_ARGS__); \
    } \
    return ret; \
  }
#define GENERATE_FREE_FUNC(Class) \
  static inline void Class ## _free (void *this) { \
    Class ## _destory(this); \
    free(this); \
  }


#endif /* CLASS_H */
