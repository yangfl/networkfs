#define FILL_DIR \
  filler(buf, ".", NULL, 0, 0); \
  filler(buf, "..", NULL, 0, 0);

#define FUSE_EXIT() fuse_exit(fuse_get_context()->fuse)

#define FUSE_LOAD_CONTEXT() \
  struct fuse_context *ctx = fuse_get_context()

