#include <string.h>

#include <grammar/try.h>
#include "buffer.h"


extern inline void Buffer_destory (struct Buffer* this);


static size_t Buffer_resize (struct Buffer *this, size_t new_size) {
  if (this->extensible) {
    do_once {
      char *new_data = (char *) realloc(this->data, new_size);
      condition_throw(new_data) MallocException("buffer resize");
      this->size = new_size;
      this->data = new_data;
    }
  }

  return this->size;
}


size_t Buffer_append (char *ptr, size_t size, size_t nmemb, void *data) {
  if unlikely (data == NULL || nmemb == 0) {
    return nmemb;
  }
  struct Buffer *this = (struct Buffer *) data;

  size_t new_size = this->offset + nmemb;
  if (new_size > this->size) {
    nmemb = Buffer_resize(this, new_size) - this->offset;
  }
  memcpy(this->data + this->offset, ptr, nmemb);
  this->offset += nmemb;
  return nmemb;
}


size_t Buffer_fetch (char *ptr, size_t size, size_t nitems, void *data) {
  if unlikely (data == NULL || nitems == 0) {
    return nitems;
  }
  struct Buffer *this = (struct Buffer *) data;

  if (this->offset + nitems > this->size) {
    nitems = this->size - this->offset;
  }
  memcpy(ptr, this->data + this->offset, nitems);
  this->offset += nitems;
  return nitems;
}


int Buffer_init_exist (struct Buffer *this, char *data, size_t size, bool extensible) {
  this->data = data;
  this->size = size;
  this->offset = 0;
  this->extensible = extensible;

  return 0;
}


int Buffer_init_new (struct Buffer *this, size_t size) {
  try {
    throwable this->data = malloc_e(size, "buffer data");
    this->size = size;
    this->offset = 0;
    this->extensible = true;
  } onerror (e) {
    free(this->data);
  }

  return TEST_SUCCESS;
}
