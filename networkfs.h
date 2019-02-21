#ifndef NETWORKFS_H
#define NETWORKFS_H

#include <stdio.h>

#include <grammar/exception.h>


#define NETWORKFS_NAME "networkfs"
#define NETWORKFS_VERSION "0.0"
#define NETWORKFS_URL "https://github.com/yangfl/networkfs/"
#define DEFAULT_CACHE_TIMEOUT 2


typedef Exception NetworkFSException;
VTABLE_IMPL(Exception, NetworkFSException);

inline int NetworkFSException_init (
    NetworkFSException *e, const char *file, const char *func, unsigned line,
    const char *what) {
  int res = Exception_init_what((Exception *) e, file, func, line, what);
  if unlikely (res) {
    return res;
  }

  e->VTABLE(Exception) = &VTABLE_OF(Exception, NetworkFSException);
  return 0;
}

#define NetworkFSException(msg) GENERATE_EXCEPTION_INIT(NetworkFSException, msg)


#endif /* NETWORKFS_H */
