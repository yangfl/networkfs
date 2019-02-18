#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include "dummy.h"


static int dummy_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
       off_t offset, struct fuse_file_info *fi,
       enum fuse_readdir_flags flags) {
  if (strcmp(path, "/") != 0)
    return -ENOENT;

  filler(buf, ".", NULL, 0, 0);
  filler(buf, "..", NULL, 0, 0);
  filler(buf, "hello", NULL, 0, 0);

  return 0;
}


int dummy_getattr(const char *path, struct stat *stbuf,
           struct fuse_file_info *fi) {
  int res = 0;

  memset(stbuf, 0, sizeof(struct stat));
  if (strcmp(path, "/") == 0) {
    stbuf->st_mode = S_IFDIR | 0755;
    stbuf->st_nlink = 2;
  } else if (strcmp(path+1, "hello") == 0) {
    stbuf->st_mode = S_IFREG | 0444;
    stbuf->st_nlink = 1;
    stbuf->st_size = strlen("Hello!");
  } else
    res = -ENOENT;

  return res;
}


int dummy_read(const char *path, char *buf, size_t size, off_t offset,
        struct fuse_file_info *fi) {
  size_t len;
  if(strcmp(path+1, "hello") != 0)
    return -ENOENT;

  len = strlen("Hello!");
  if (offset < (off_t) len) {
    if (offset + size > len)
      size = len - offset;
    memcpy(buf, "Hello!" + offset, size);
  } else
    size = 0;

  return size;
}


int dummy_getoper (struct proto_operations *proto_oper, struct networkfs_opts *options) {
  proto_oper->readdir         = dummy_readdir;
  proto_oper->getattr         = dummy_getattr;
  proto_oper->read            = dummy_read;
  return 0;
}
