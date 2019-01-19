#ifndef NETWORKFS_BUFFER_H
#define NETWORKFS_BUFFER_H

#include <stdbool.h>
#include <stdlib.h>

#include <wrapper/log.h>
#include <grammar/macro.h>
#include <grammar/class.h>


typedef struct Buffer {
  char *data;
  size_t size;
  off_t offset;
  bool extensible;
} Buffer;


size_t Buffer_append (char *ptr, size_t size, size_t nmemb, void *data);
size_t Buffer_fetch (char *ptr, size_t size, size_t nitems, void *data);

inline void Buffer_destory (struct Buffer* this) {
  PROTECT_RETURN(this);

  if (this->extensible) {
    free(this->data);
  }
}

int Buffer_init_exist (struct Buffer *this, char *data, size_t size, bool extensible);
int Buffer_init_new (struct Buffer *this, size_t size);
#define Buffer_init_quick(this, ...) Buffer_init_new(this, 512)
#define Buffer(...) GET_5TH_ARG( \
    __VA_ARGS__, Buffer_init_exist, arg4, Buffer_init_new, Buffer_init_quick, \
  )(__VA_ARGS__)

GENERATE_NEW_FUNC(Buffer, struct Buffer, exist, (char *data, size_t size, bool extensible), data, size, extensible)
GENERATE_NEW_FUNC(Buffer, struct Buffer, new, (size_t size), size)
GENERATE_NEW_FUNC(Buffer, struct Buffer, quick, (), )
#define Buffer_new(...) GET_5TH_ARG( \
    arg0, ## __VA_ARGS__, Buffer_new_exist, arg4, Buffer_new_new, Buffer_new_quick, \
  )(__VA_ARGS__)

GENERATE_FREE_FUNC(Buffer)


#endif /* NETWORKFS_BUFFER_H */
