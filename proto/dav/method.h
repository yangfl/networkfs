#ifndef PROTO_DAV_METHOD_H
#define PROTO_DAV_METHOD_H

#include <stdbool.h>

#ifndef FUSE_USE_VERSION
#define FUSE_USE_VERSION 31
#endif

#include <fuse.h>
#include <curl/curl.h>

#include <opts.h>


#define DAV_METHOD \
  X(OPTIONS) \
  X(GET) \
  X(HEAD) \
  X(POST) \
  X(PUT) \
  X(DELETE) \
  X(TRACE) \
  X(COPY) \
  X(MOVE) \
  X(MKCOL) \
  X(PROPFIND) \
  X(PROPPATCH) \
  X(LOCK) \
  X(UNLOCK) \
  X(ORDERPATCH) \

struct DavServer {
  CURLU *baseuh;
  const struct networkfs_opts *options;
  void *filelock_tree;
  pthread_rwlock_t filelock_tree_lock;
  char *server;
  char *version;
#define X(o) bool o;
  DAV_METHOD
#undef X
};

enum LockType {
  RW_LOCK_NONE = 0,
  RW_LOCK_READ,
  RW_LOCK_WRITE,
};


int __dav_method (
    struct DavServer *server, const char *path, const char *method, enum LockType lock_type);

inline int dav_mkcol (struct DavServer *server, const char *path) {
  return __dav_method(server, path, "MKCOL", RW_LOCK_NONE);
}

inline int dav_put_simple (struct DavServer *server, const char *path) {
  return __dav_method(server, path, "PUT", RW_LOCK_READ);
}

inline int dav_delete (struct DavServer *server, const char *path) {
  return __dav_method(server, path, "DELETE", RW_LOCK_WRITE);
}

int dav_head (struct DavServer *server, const char *path, size_t *sizep);
ssize_t dav_get (struct DavServer *server, const char *path, char *data, size_t size, off_t offset);
int dav_put (struct DavServer *server, const char *path, const char *data, size_t size, off_t offset);

int __dav_move (struct DavServer *server, const char *from, const char *to, bool nooverwrite);

inline int dav_move (struct DavServer *server, const char *from, const char *to) {
  return __dav_move(server, from, to, false);
}

inline int dav_move_nooverwrite (struct DavServer *server, const char *from, const char *to) {
  return __dav_move(server, from, to, true);
}

int dav_copy (struct DavServer *server, const char *from, const char *to);
int dav_propfind (struct DavServer *server, const char *path, int depth, void *buf, fuse_fill_dir_t filler);
int dav_proppatch (struct DavServer *server, const char *path, const char *key, const char *value);
int dav_lock (struct DavServer *server, const char *path);
int dav_unlock (struct DavServer *server, const char *path);
int dav_options (struct DavServer *server);
void dav_destory (struct DavServer *server);

#endif /* PROTO_DAV_METHOD_H */
