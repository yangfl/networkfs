#ifndef NETWORKFS_PROTO_H
#define NETWORKFS_PROTO_H

#ifndef FUSE_USE_VERSION
#define FUSE_USE_VERSION 31
#endif

#include <fuse.h>

#include <opts.h>


struct proto_operations {
  struct fuse_operations;

  int (*readall)(const char *path, char *buf, size_t size, struct fuse_file_info *fi);
  int (*writeall)(const char *path, const char *buf, size_t size, struct fuse_file_info *fi);
};


struct proto_operations *get_proto_oper (const char *scheme, struct networkfs_opts *options);


#endif /* NETWORKFS_PROTO_H */
