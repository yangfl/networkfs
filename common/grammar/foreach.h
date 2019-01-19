#ifndef FOREACH_H
#define FOREACH_H

#include "macro.h"


#define foreach_strtok(varname, iterable, delim) \
  for (char *CONCAT(__foreach_saveptr_, varname), *varname = strtok_r(iterable, delim, &CONCAT(__foreach_saveptr_, varname)); \
       varname; \
       varname = strtok_r(NULL, delim, &CONCAT(__foreach_saveptr_, varname)))
#define foreach_range_3(varname, start, stop, step) \
  for (auto varname = start; start < stop ? varname < stop : varname > stop; varname += step)
#define foreach_range_2(varname, start, stop) \
  for (auto varname = start; varname < stop; varname++)
#define foreach_range_1(varname, stop) \
  for (auto varname = 0; varname < stop; varname++)
#define foreach_range_generic(...) GET_5TH_ARG(__VA_ARGS__, foreach_range_3, foreach_range_2, foreach_range_1, )
#define forrange(...) foreach_range_generic(__VA_ARGS__)(__VA_ARGS__)
#define foreach_range(...) foreach_range_generic(__VA_ARGS__)(__VA_ARGS__)
#define foreach_LinkedList(varname, iterable) \
  for (typeof(iterable) varname = iterable; varname; varname = varname->next)
#define foreach_Node(varname, iterable) \
  for (typeof(iterable->children) varname = iterable->children; varname; varname = varname->next)
#define foreach(type) foreach_ ## type


#endif /* FOREACH_H */
