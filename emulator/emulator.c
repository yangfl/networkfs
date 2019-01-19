#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/stat.h>

#include "emulator.h"


struct fuse_operations *emulate_networkfs_oper (struct proto_operations *proto_oper) {
  #define FILL_OPER(oper) \
    if (proto_oper->oper == NULL) { \
      proto_oper->oper = emulate_networkfs_ ## oper(); \
    }

  /*
  FILL_OPER(readdir);
  FILL_OPER(getattr);
  FILL_OPER(read);
  FILL_OPER(writeall);

  #undef FILL_OPER
  */

  return (struct fuse_operations *) proto_oper;
}
