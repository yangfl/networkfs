#include <string.h>

#include <grammar/try.h>

#include "../networkfs.h"
#include "dav/dav.h"
#include "dummy/dummy.h"

#include "proto.h"



static struct proto_operations proto_oper = {0};


struct proto_operations *get_proto_oper (const char *scheme, struct networkfs_opts *options) {
  struct proto_operations *ret = NULL;

  #define ADD_PROTO(oper) \
    else if (strcmp(scheme, # oper) == 0) { \
      if (oper ## _getoper(&proto_oper, options) == 0) { \
        ret = &proto_oper; \
      } \
    }

  if (0) {
  }
  ADD_PROTO(dav)
  //ADD_PROTO(ftp)
  //ADD_PROTO(http)
  ADD_PROTO(dummy)
  else {
    char msg[sizeof("unsupported scheme `'") + strlen(scheme)];
    snprintf(msg, sizeof(msg), "unsupported scheme `%s'", scheme);
    NetworkFSException(msg);
    return NULL;
  }
  #undef ADD_PROTO

  should (ret) otherwise {
    if (!Exception_has(&ex)) {
      char msg[sizeof("scheme `' has no handler") + strlen(scheme)];
      snprintf(msg, sizeof(msg), "scheme `%s' has no handler", scheme);
      NetworkFSException(msg);
    }
    return NULL;
  }

  return ret;
}
