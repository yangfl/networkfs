#define _GNU_SOURCE  // RENAME_NOREPLACE

#include <errno.h>
#include <stdio.h>

#include <utils.h>
#include <wrapper/log.h>
#include <grammar/try.h>
#include <template/buffer.h>
#include <wrapper/curl.h>
#include <wrapper/fuse.h>
#include "dav.h"
#include "method.h"
#include "parser.h"

#ifdef DEBUG
#define DBG printf
#else
#define DBG(...)
#endif

static struct DavServer server = {0};


static void __attribute__((constructor)) dav_load (void) {
  LIBXML_TEST_VERSION
  if (!xmlHasFeature(XML_WITH_THREAD)) {
    exit(1);
  }
}


static inline int dav_exception_map () {
  int res;

  do_once {
    if (issubtype(Exception, &ex, CurlException) && ((CurlException *) &ex)->code == CURLE_HTTP_RETURNED_ERROR) {
      switch (((CurlException *) &ex)->response_code) {
        case 404:  /* Not Found */
          res = -ENOENT;
          break;
        case 412:  /* Precondition Failed */
          res = -EEXIST;
          break;
        case 423:  /* Locked */
          res = -EBUSY;
          break;
        case 501:  /* Not Implemented */
          res = -EOPNOTSUPP;
          break;
        case 507:  /* Insufficient Storage */
          res = -ENOSPC;
          break;
        default:
          goto not_mapped_errcode;
      }
      break;
      not_mapped_errcode:;
    }

    Exception_fputs(&ex, stderr);

    if (issubtype(Exception, &ex, MallocException)) {
      res = -ENOMEM;
    } else {
      res = -EIO;
    }
  }
  Exception_destory(&ex);
  return res;
}


static inline int dav_exception_test (int res) {
  if unlikely (res) {
    return dav_exception_map();
  } else {
    return 0;
  }
}


static inline int dav_exception_check (int res) {
  if unlikely (Exception_has(&ex)) {
    return dav_exception_map();
  } else {
    return res;
  }
}


static int dav_readdir (const char *path, void *buf, fuse_fill_dir_t filler,
                        off_t offset, struct fuse_file_info *fi,
                        enum fuse_readdir_flags flags) {
    DBG("dav_readdir %s\n",path);
  filler(buf, "..", NULL, 0, 0);
  dav_propfind(&server, path, 1, buf, filler);
  return dav_exception_check(0);
}


static int dav_getattr_callback (
    void *buf, const char *name, const struct stat *stbuf,
    off_t off, enum fuse_fill_dir_flags flags) {
  memcpy(buf, stbuf, sizeof(struct stat));
  return 0;
}


static int dav_getattr (const char *path, struct stat *stbuf,
                        struct fuse_file_info *fi) {
    DBG("dav_getattr %s\n",path);
  dav_propfind(&server, path, 0, stbuf, dav_getattr_callback);
  return dav_exception_check(0);
}


static int dav_read (const char *path, char *data, size_t size, off_t offset,
                     struct fuse_file_info *fi) {
    DBG("dav_read %s %zd+%zd\n",path,offset,size);
  if unlikely (size == 0) {
    return 0;
  }

  ssize_t read_size = dav_get(&server, path, data, size, offset);
  if (read_size < 0) {
    if (issubtype(Exception, &ex, CurlException) &&
        ((CurlException *) &ex)->code == CURLE_HTTP_RETURNED_ERROR &&
        ((CurlException *) &ex)->response_code == 416) {
      Exception_destory(&ex);
      return 0;
    } else {
      return dav_exception_map();
    }
  }

  return read_size;
}


static int dav_write (const char *path, const char *data, size_t size,
                      off_t offset, struct fuse_file_info *fi) {
    DBG("dav_write %s %zd+%zd\n",path,offset,size);
  int res;

  size_t real_size;
  res = dav_head(&server, path, &real_size);
  if (res) {
    return dav_exception_map();
  }
  if (real_size < (unsigned) offset) {
    char buf[offset - real_size];
    memset(buf, 0, sizeof(buf));
    res = dav_put(&server, path, buf, sizeof(buf), real_size);
    if (res) {
      return dav_exception_map();
    }
  }
  dav_put(&server, path, data, size, offset);
  return dav_exception_check(size);
}


static int dav_mkdir (const char *path, mode_t mode) {
    DBG("dav_mkdir %s\n",path);
  if unlikely (!server.MKCOL) {
    return -EOPNOTSUPP;
  }

  dav_mkcol(&server, path);
  return dav_exception_check(0);
}

static int dav_rename (const char *from, const char *to, unsigned int flags) {
    DBG("dav_rename %s -> %s\n",from,to);
  if unlikely (!server.MOVE) {
    return -EOPNOTSUPP;
  }

  switch (flags) {
    case 0:
      dav_move(&server, from, to);
      break;
    case RENAME_NOREPLACE:
      dav_move_nooverwrite(&server, from, to);
      break;
    default:
      return -EINVAL;
  }

  return dav_exception_check(0);
}


static int dav_unlink (const char *path) {
    DBG("dav_unlink %s\n",path);
  dav_delete(&server, path);
  return dav_exception_check(0);
}


static int dav_chmod (const char *path, mode_t mode, struct fuse_file_info *fi) {
    DBG("dav_chmod %s %o\n",path,mode);
  char value[19];
  snprintf(value, sizeof(value), "%o", mode);
  dav_proppatch(&server, path, "N:mode", value);
  return dav_exception_check(0);
}


static int dav_chown (const char *path, uid_t uid, gid_t gid, struct fuse_file_info *fi) {
    DBG("dav_chown %s %d:%d\n",path,uid,gid);
  char value[22];
  snprintf(value, sizeof(value), "%d:%d", uid, gid);
  dav_proppatch(&server, path, "N:owner", value);
  return dav_exception_check(0);
}


static int dav_mknod (const char *path, mode_t mode, dev_t rdev) {
    DBG("dav_mknod %s\n",path);
  if (!S_ISREG(mode)) {
    return -EINVAL;
  }

  try {
    throwable try {
      throwable dav_head(&server, path, NULL);
      return -EEXIST;
    } catch (CurlException, e) {
      if (!(e->code == CURLE_HTTP_RETURNED_ERROR && (e->response_code == 404 || e->response_code == 403))) {
        throw;
      }
    } onerror (e) { }
    dav_put_simple(&server, path);
  } onerror (e) {
    return dav_exception_map();
  }

  return dav_chmod(path, mode, NULL);
}


static int dav_truncate (const char *path, off_t size, struct fuse_file_info *fi) {
    DBG("dav_truncate %s %zd\n",path,size);
  int res;

  struct stat st;
  res = dav_getattr(path, &st, NULL);
  if (res) {
    return res;
  }
  if (st.st_size == size) {
    return 0;
  }

  if (size == 0) {
    res = dav_delete(&server, path);
    if (res) {
      return dav_exception_map();
    }
    res = dav_put_simple(&server, path);
    if (res) {
      return dav_exception_map();
    }
    res = dav_chmod(path, st.st_mode, fi);
  } else {
    size_t real_size;
    res = dav_head(&server, path, &real_size);
    if (res) {
      return dav_exception_map();
    }
    if ((unsigned) size < real_size / 2) {
      char buf[size];
      res = dav_read(path, buf, size, 0, fi);
      if (res) {
        return res;
      }
      res = dav_unlink(path);
      if (res) {
        return res;
      }
      res = dav_put(&server, path, buf, size, 0);
    } else {
      char s_size[11];
      snprintf(s_size, sizeof(s_size), "%zd", size);
      res = dav_proppatch(&server, path, "size", s_size);
    }
    res = dav_exception_test(res);
  }

  return res;
}



static inline int __dav_open (const char *path) {
  if (!server.LOCK) {
    return 0;
  }

  return dav_exception_test(dav_lock(&server, path));
}


static int dav_open (const char *path, struct fuse_file_info *fi) {
    DBG("dav_open %s\n",path);
  int res = 0;

  if (fi->flags & O_WRONLY || fi->flags & O_RDWR) {
    if (fi->flags & O_TRUNC) {
      res = dav_truncate(path, 0, fi);
    }
    if (res == 0 || (fi->flags & O_CREAT && res == -ENOENT)) {
      res = __dav_open(path);
    }
  }

  return res;
}


static int dav_create (const char *path, mode_t mode, struct fuse_file_info *fi) {
  int res;

  res = dav_mknod(path, mode, 0);
  if (res) {
    return res;
  }

  if (fi->flags & O_WRONLY || fi->flags & O_RDWR) {
    res = __dav_open(path);
  }

  return res;
}


static int dav_release (const char *path, struct fuse_file_info *fi) {
    DBG("dav_release %s\n",path);
  if (!server.LOCK) {
    return 0;
  }
  if (!(fi->flags & O_WRONLY || fi->flags & O_RDWR)) {
    return 0;
  }

  dav_unlock(&server, path);
  return 0;
}


static ssize_t dav_copy_file_range (
    const char *path_in, struct fuse_file_info *fi_in, off_t offset_in,
    const char *path_out, struct fuse_file_info *fi_out, off_t offset_out,
    size_t len, int flags) {
    DBG("dav_copy_file_range %s -> %s\n",path_in,path_out);
  if (offset_in != offset_out) {
    return -EOPNOTSUPP;
  }

  int res;
  struct stat st_out = {.st_size = 0};
  res = dav_getattr(path_out, &st_out, fi_out);
  if (res && res != -ENOENT) {
    return res;
  }
  if (len < (unsigned) st_out.st_size / 2) {
    return -EOPNOTSUPP;
  }

  char file_out_before[offset_out];
  if (sizeof(file_out_before)) {
    res = dav_read(path_out, file_out_before, sizeof(file_out_before), 0, fi_out);
    if (res) {
      return res;
    }
  }
  char file_out_after[offset_out + len < (unsigned) st_out.st_size ? st_out.st_size - len - offset_out : 0];
  if (sizeof(file_out_after)) {
    res = dav_read(path_out, file_out_after, sizeof(file_out_after), offset_out, fi_out);
    if (res) {
      return res;
    }
  }

  res = dav_exception_test(dav_copy(&server, path_in, path_out));
  if (res) {
    return res;
  }

  if (sizeof(file_out_before)) {
    res = dav_write(path_out, file_out_before, sizeof(file_out_before), 0, fi_out);
    if (res) {
      return res;
    }
  }
  if (sizeof(file_out_after)) {
    res = dav_write(path_out, file_out_after, sizeof(file_out_after), offset_out, fi_out);
    if (res) {
      return res;
    }
  }

  return len;
}


static void *dav_init (struct fuse_conn_info *conn, struct fuse_config *cfg) {
  try {
    xmlInitParser();

    throwable dav_options(&server);
    DBG("The server is: %s/%s\n", server.server, server.version);
    DBG("The server support:");
  #define X(o) if (server.o) DBG(" " # o);
    DAV_METHOD
  #undef X
    DBG("\n");
  } catch (e) {
    Exception_fputs(e, stderr);
    FUSE_EXIT();
  }

  return NULL;
}


static void dav_destroy (void *private_data) {
  dav_destory(&server);
}


int dav_getoper (struct proto_operations *proto_oper, struct networkfs_opts *options) {
  try {
    throwable server.baseuh = new(CURLU) (options->baseurl);
    curl_url_set_or_die(server.baseuh, CURLUPART_SCHEME, options->use_ssl == CURLUSESSL_NONE ? "http" : "https", 0);
    //UnspecifiedException("aaa");
  } onerror (e) {
    delete(CURLU) server.baseuh;
    server.baseuh = NULL;
    return 1;
  }

  server.options = options;

  proto_oper->init            = dav_init;
  proto_oper->destroy         = dav_destroy;
  proto_oper->readdir         = dav_readdir;
  proto_oper->getattr         = dav_getattr;
  proto_oper->chmod           = dav_chmod;
  proto_oper->chown           = dav_chown;
  proto_oper->read            = dav_read;
  proto_oper->write           = dav_write;
  proto_oper->mknod           = dav_mknod;
  proto_oper->mkdir           = dav_mkdir;
  proto_oper->rename          = dav_rename;
  proto_oper->unlink          = dav_unlink;
  proto_oper->rmdir           = dav_unlink;
  proto_oper->create          = dav_create;
  proto_oper->open            = dav_open;
  proto_oper->release         = dav_release;
  proto_oper->truncate        = dav_truncate;
  proto_oper->copy_file_range = dav_copy_file_range;

  return 0;
}
