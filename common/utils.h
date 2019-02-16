#ifndef NETWORKFS_UTILS_H
#define NETWORKFS_UTILS_H

#include <ctype.h>
#include <string.h>


#define CONSTSTR(s) s, strlen(s)
#define strscmp(str1, str2) strncmp(str1, str2, strlen(str2))


inline char *strnrstrip (char *s, size_t n) {
  char *end = s + n - 1;
  while (end >= s && isspace(*end)) {
    end--;
  }
  *(end + 1) = '\0';
  return s;
}


#endif /* NETWORKFS_UTILS_H */
