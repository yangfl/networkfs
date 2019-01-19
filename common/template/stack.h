#ifndef LFSTACK_H
#define LFSTACK_H

#include <stdbool.h>
#include <stddef.h>
#include <pthread.h>


typedef struct LinkedListHead {
  struct LinkedListHead *next;
} LinkedListHead;

typedef struct LinkedList {
  struct LinkedListHead;
  void *value;
} LinkedList;

typedef struct Stack {
  struct LinkedListHead;
  size_t size;
  pthread_spinlock_t lock;
} Stack;

void Stack_push (Stack *this, LinkedListHead *node);
LinkedListHead *Stack_pop (Stack *this);

inline void Stack_destory (Stack *this) {
  pthread_spin_destroy(&this->lock);
}

inline int Stack_init (Stack *this) {
  this->next = NULL;
  this->size = 0;
  return pthread_spin_init(&this->lock, 0);
}


#endif
