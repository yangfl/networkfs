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


void print_trace (void) {
  void *array[32];
  size_t size = backtrace(array, 32) - 3;
  char **strings = backtrace_symbols(array + 3, size);

  for (size_t i = 0; i < size; i++) {
     printf("%s\n", strings[i]);
   }

  free(strings);
}


int VTable_crash (void) {
  printf("VTableCrash!\n");
  print_trace();
  exit(255);
}
