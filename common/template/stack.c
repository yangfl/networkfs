#include <grammar/try.h>
#include <grammar/synchronized.h>

#include "stack.h"


extern inline void Stack_destory (Stack *this);
extern inline int Stack_init (Stack *this);


void Stack_push (Stack *this, LinkedListHead *node) {
  synchronized (spin, &this->lock, lock) {
    node->next = this->next;
    this->next = node;
    this->size++;
  }
}


LinkedListHead *Stack_pop (Stack *this) {
  LinkedListHead *res;

  ensure_synchronized (spin, &this->lock, lock) {
    res = this->next;
    this->next = res->next;
    res->next = NULL;
    this->size--;
  } else {
    if (this->next == NULL) {
      res = NULL;
      break;
    }
  }

  return res;
}
