#ifndef NETWORKFS_SIMPLE_STRING_H
#define NETWORKFS_SIMPLE_STRING_H

#include <string.h>

#include <grammar/class.h>
#include <grammar/try.h>


typedef struct SimpleString {
  size_t len;
  char str[];
} SimpleString;

inline SimpleString *SimpleString_new (const char *str) {
  SimpleString *this;
  do_once {
    throwable this = malloc_e(sizeof(SimpleString) + strlen(str) + 1, "SimpleString");
    this->len = strlen(str);
    strcpy(this->str, str);
  }
  return this;
}

inline void SimpleString_destory (SimpleString *this) {
}

GENERATE_FREE_FUNC(SimpleString)


#endif /* NETWORKFS_SIMPLE_STRING_H */

