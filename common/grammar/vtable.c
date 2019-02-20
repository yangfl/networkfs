#include <execinfo.h>
#include <time.h>
#include <stdlib.h>
#include <stdio.h>

#include "vtable.h"


unsigned int VTable_rand = 0;


extern inline unsigned int VTable_checksum (const struct VTable *this, unsigned int size);
extern inline bool VTable_vaild (const struct VTable *this, unsigned int size);
extern inline int VTable_init (struct VTable *this, unsigned int size);


void VTable_rand_init (void) {
  if unlikely (VTable_rand == 0) {
    srand(time(0));
    do {
      VTable_rand = rand();
    } while (VTable_rand == 0);
  }
}


static inline void print_trace (void) {
  void *bt[32];
  backtrace_symbols_fd(bt + 3, backtrace(bt, sizeof(bt)) - 3, fileno(stderr));
}


int VTable_crash (void) {
  printf("VTableCrash!\n");
  print_trace();
  exit(255);
}
