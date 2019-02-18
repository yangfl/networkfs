#define _GNU_SOURCE  // tdestroy

#include <search.h>
#include <stdatomic.h>
#include <string.h>

#include <utils.h>
#include <grammar/try.h>
#include <template/buffer.h>
#include <wrapper/curl.h>
#include <grammar/foreach.h>
#include <grammar/synchronized.h>
#include <template/simple_string.h>
#include "../../networkfs.h"
#include "parser.h"
#include "method.h"

#define CONTENT_TYPE_XML "Content-Type: application/xml; charset=\"utf-8\""
#define PREFER_MINIMAL "Prefer: return=minimal"
#define NETWORKFS_XML_NS "NETWORKFS:"


struct FileLock {
  char *path;
  SimpleString *token;
  atomic_int cnt;
};


static inline void FileLock_destory (struct FileLock *this) {
  free(this->path);
  delete(SimpleString) this->token;
}

GENERATE_FREE_FUNC(FileLock)


static int __dav_strpcmp (const void *a, const void *b) {
  return strcmp(*((const char **) a), *((const char **) b));
}


static struct curl_slist *__dav_header_if (
    struct DavServer *server, struct curl_slist *list, const char *path, enum LockType lock_type) {
  struct FileLock **filelock_p = tfind(&path, &server->filelock_tree, __dav_strpcmp);
  if (filelock_p) {
    struct FileLock *filelock = *filelock_p;
    do_once {
      char if_header[sizeof("If: (<>)") + filelock->token->len];
      snprintf(if_header, sizeof(if_header), "If: (<%s>)", filelock->token->str);
      list = curl_slist_append_e(list, if_header);
      if (list == NULL) {
        break;
      }
      if (lock_type == RW_LOCK_WRITE) {
        tdelete(&path, &server->filelock_tree, __dav_strpcmp);
        delete(FileLock) filelock;
      }
    }
  }

  return list;
}


int __dav_method (
    struct DavServer *server, const char *path, const char *method, enum LockType lock_type) {
  with_curl (curl, server->baseuh, path, server->options) {
    if (server->options->use_lock && lock_type) {
      switch (lock_type) {
        case RW_LOCK_NONE:
          break;
        case RW_LOCK_READ:
          while (pthread_rwlock_rdlock(&server->filelock_tree_lock));
          break;
        case RW_LOCK_WRITE:
          while (pthread_rwlock_wrlock(&server->filelock_tree_lock));
          break;
      }
    }

    throwable scope (curl_slist, list) {
      if (server->options->use_lock && lock_type) {
        throwable list = __dav_header_if(server, list, path, lock_type);
        curl_easy_setopt_or_die(curl, CURLOPT_HTTPHEADER, list);
      }
      curl_easy_setopt_or_die(curl, CURLOPT_CUSTOMREQUEST, method);
      curl_easy_setopt_or_die(curl, CURLOPT_NOBODY, 1L);
      curl_easy_perform_or_die(curl);
    }

    if (server->options->use_lock && lock_type) {
      pthread_rwlock_unlock(&server->filelock_tree_lock);
    }
  }
  return TEST_SUCCESS;
}


extern inline int dav_mkcol (struct DavServer *server, const char *path);
extern inline int dav_put_simple (struct DavServer *server, const char *path);
extern inline int dav_delete (struct DavServer *server, const char *path);


static size_t __dav_head_callback (char *buffer, size_t size, size_t nitems, void *userdata) {
  size_t *sizep = (size_t *) userdata;

  if (strscmp(buffer, "Content-Length:") == 0) {
    buffer += strlen("Content-Length:");
    buffer = strlstrip(buffer);
    *sizep = strtol(buffer, NULL, 10);
  }

  return nitems;
}


int dav_head (struct DavServer *server, const char *path, size_t *sizep) {
  with_curl (curl, server->baseuh, path, server->options) {
    curl_easy_setopt_or_die(curl, CURLOPT_CUSTOMREQUEST, "HEAD");
    curl_easy_setopt_or_die(curl, CURLOPT_NOBODY, 1L);
    if (sizep) {
      *sizep = 0;
      curl_easy_setopt_or_die(curl, CURLOPT_HEADERDATA, sizep);
      curl_easy_setopt_or_die(curl, CURLOPT_HEADERFUNCTION, __dav_head_callback);
    }
    curl_easy_perform_or_die(curl);
  }
  return TEST_SUCCESS;
}


ssize_t dav_get (struct DavServer *server, const char *path, char *data, size_t size, off_t offset) {
  int res;
  with (Buffer buf, Buffer(&buf, data, size, false), Buffer_destory(&buf)) {
    with_curl (curl, server->baseuh, path, server->options) {
      if (offset != 0 || size != 0) {
        throwable with_range (range, offset, size) {
          curl_easy_setopt_or_die(curl, CURLOPT_RANGE, range);
        }
      }
      curl_easy_setopt_or_die(curl, CURLOPT_WRITEDATA, &buf);
      curl_easy_setopt_or_die(curl, CURLOPT_WRITEFUNCTION, Buffer_append);
      curl_easy_perform_or_die(curl);
    }
    res = TEST_SUCCESS == 0 ? buf.offset : -TEST_SUCCESS;
  }
  return res;
}


int dav_put (struct DavServer *server, const char *path, const char *data, size_t size, off_t offset) {
  with (Buffer buf, Buffer(&buf, (char *) data, size, false), Buffer_destory(&buf)) {
    throwable with_curl (curl, server->baseuh, path, server->options) {
      throwable scope (curl_slist, list) {
        if (server->options->use_lock) {
          while (pthread_rwlock_rdlock(&server->filelock_tree_lock));
          throwable list = __dav_header_if(server, list, path, 1);
          curl_easy_setopt_or_die(curl, CURLOPT_HTTPHEADER, list);
        }
        curl_easy_setopt_or_die(curl, CURLOPT_UPLOAD, 1L);
        curl_easy_setopt_or_die(curl, CURLOPT_READDATA, &buf);
        curl_easy_setopt_or_die(curl, CURLOPT_READFUNCTION, Buffer_fetch);
        throwable with_range (range, offset, size) {
          curl_easy_setopt_or_die(curl, CURLOPT_RANGE, range);
        }
        curl_easy_setopt_or_die(curl, CURLOPT_INFILESIZE, (long) size);
        curl_easy_perform_or_die(curl);
        if (server->options->use_lock) {
          pthread_rwlock_unlock(&server->filelock_tree_lock);
        }
      }
    }
  }
  return TEST_SUCCESS;
}


static struct curl_slist *__dav_header_destination (
    struct DavServer *server, struct curl_slist *list, const char *path) {
  try {
    throwable scope (CURLU, path_uh, server->baseuh) {
      if likely (path[0] == '/') {
        path++;
      }
      curl_url_set_or_die(path_uh, CURLUPART_URL, path, CURLU_URLENCODE);
      throwable with (curl_char *path_url = NULL,
                      curl_url_get_or_die(path_uh, CURLUPART_URL, &path_url, 0),
                      curl_free(path_url)) {
        char dest_header[sizeof("Destination: ") + strlen(path_url)];
        snprintf(dest_header, sizeof(dest_header), "Destination: %s", path_url);
        throwable list = curl_slist_append_e(list, dest_header);
      }
    }
  } onerror (e) {
    curl_slist_free(list);
    return NULL;
  }
  return list;
}


int __dav_move (struct DavServer *server, const char *from, const char *to, bool nooverwrite) {
  with_curl (curl, server->baseuh, from, server->options) {
    throwable scope (curl_slist, list) {
      throwable list = __dav_header_destination(server, list, to);
      if (nooverwrite) {
        throwable list = curl_slist_append_e(list, "Overwrite: F");
      }

      if (server->options->use_lock) {
        while (pthread_rwlock_rdlock(&server->filelock_tree_lock));
        if (tfind(&from, &server->filelock_tree, __dav_strpcmp)) {
          pthread_rwlock_unlock(&server->filelock_tree_lock);
          while (pthread_rwlock_wrlock(&server->filelock_tree_lock));
          throwable list = __dav_header_if(server, list, from, RW_LOCK_WRITE);
        }
        throwable list = __dav_header_if(server, list, to, RW_LOCK_READ);
      }
      curl_easy_setopt_or_die(curl, CURLOPT_HTTPHEADER, list);
      curl_easy_setopt_or_die(curl, CURLOPT_CUSTOMREQUEST, "MOVE");
      curl_easy_setopt_or_die(curl, CURLOPT_NOBODY, 1L);
      curl_easy_perform_or_die(curl);
      if (server->options->use_lock) {
        pthread_rwlock_unlock(&server->filelock_tree_lock);
      }
    }
  }
  return TEST_SUCCESS;
}


extern inline int dav_move (struct DavServer *server, const char *from, const char *to);
extern inline int dav_move_nooverwrite (struct DavServer *server, const char *from, const char *to);


int dav_copy (struct DavServer *server, const char *from, const char *to) {
  with_curl (curl, server->baseuh, from, server->options) {
    throwable scope (curl_slist, list) {
      throwable list = __dav_header_destination(server, list, to);
      if (server->options->use_lock) {
        while (pthread_rwlock_rdlock(&server->filelock_tree_lock));
        throwable list = __dav_header_if(server, list, to, RW_LOCK_READ);
      }
      curl_easy_setopt_or_die(curl, CURLOPT_HTTPHEADER, list);
      curl_easy_setopt_or_die(curl, CURLOPT_CUSTOMREQUEST, "COPY");
      curl_easy_setopt_or_die(curl, CURLOPT_NOBODY, 1L);
      curl_easy_perform_or_die(curl);
      if (server->options->use_lock) {
        pthread_rwlock_unlock(&server->filelock_tree_lock);
      }
    }
  }
  return TEST_SUCCESS;
}


int dav_propfind (struct DavServer *server, const char *path, int depth, void *buf, fuse_fill_dir_t filler) {
#if 1
  const char propfind_body[] = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
  "<D:propfind xmlns:D=\"DAV:\" xmlns:N=\"" NETWORKFS_XML_NS "\">\r\n"
    "<D:prop>\r\n"
      "<D:resourcetype/>"
      "<D:creationdate/>"
      "<D:getlastmodified/>"
      "<D:getcontentlength/>"
      "<N:mode/>"
      "<N:size/>"
      "<N:owner/>"
      "<N:time/>\r\n"
    "</D:prop>\r\n"
  "</D:propfind>\r\n";
#endif

#if 1
  with (Buffer propfind_buf,
        Buffer(&propfind_buf, (char *) propfind_body, strlen(propfind_body), false),
        Buffer_destory(&propfind_buf)) {
#endif
    with (xmlParserCtxtPtr ctxt = NULL,
          if (ctxt) xmlFreeDoc(ctxt->myDoc); xmlFreeParserCtxt(ctxt)) {
      throwable with_curl (curl, server->baseuh, path, server->options) {
        char depth_header[32];
        snprintf(depth_header, sizeof(depth_header), "Depth: %d", depth);
        throwable scope (curl_slist, list, depth_header) {
          list = curl_slist_append_weak(list, PREFER_MINIMAL);
          list = curl_slist_append_weak(list, CONTENT_TYPE_XML);
          curl_easy_setopt_or_die(curl, CURLOPT_HTTPHEADER, list);
        #if 1
          curl_easy_setopt_or_die(curl, CURLOPT_UPLOAD, 1L);
          curl_easy_setopt_or_die(curl, CURLOPT_READDATA, &propfind_buf);
          curl_easy_setopt_or_die(curl, CURLOPT_READFUNCTION, Buffer_fetch);
          curl_easy_setopt_or_die(curl, CURLOPT_INFILESIZE, (long) propfind_buf.size);
        #endif
          curl_easy_setopt_or_die(curl, CURLOPT_WRITEDATA, &ctxt);
          curl_easy_setopt_or_die(curl, CURLOPT_WRITEFUNCTION, curl_parse_xml);
          curl_easy_setopt_or_die(curl, CURLOPT_CUSTOMREQUEST, "PROPFIND");
          curl_easy_perform_or_die(curl);
        }

        xmlParseChunk(ctxt, NULL, 0, 1);

        long response_code;
        curl_easy_getinfo_or_die(curl, CURLINFO_RESPONSE_CODE, &response_code);
        if (response_code != 207) {
          break;
        }

        auto doc = ctxt->myDoc;
        condition_throw(ctxt->wellFormed) UnspecifiedException("Failed to parse");

        const char *encoded_baseurl;
        curl_easy_getinfo_or_die(curl, CURLINFO_EFFECTIVE_URL, &encoded_baseurl);

        foreach(Node) (response, xmlDocGetRootElement(doc)) {
          if (xmlStrcmp(response->name, BAD_CAST "response") != 0) {
            continue;
          }

          xmlNodePtr href = NULL;
          xmlNodePtr propstat = NULL;
          xmlNodePtr prop = NULL;
          xmlNodePtr status = NULL;
          foreach(Node) (response_node, response) {
            if (href == NULL) {
              if (xmlStrcmp(response_node->name, BAD_CAST "href") == 0) {
                href = response_node;
              }
            } else if (propstat == NULL) {
              if (xmlStrcmp(response_node->name, BAD_CAST "propstat") != 0) {
                continue;
              }
              propstat = response_node;
              foreach(Node) (propstat_node, propstat) {
                if (prop == NULL) {
                  if (xmlStrcmp(propstat_node->name, BAD_CAST "prop") == 0) {
                    prop = propstat_node;
                  }
                } else if (status == NULL) {
                  if (xmlStrcmp(propstat_node->name, BAD_CAST "status") == 0) {
                    status = propstat_node;
                  }
                } else {
                  break;
                }
              }
              if (status->children == NULL ||
                  xmlStrcmp(status->children->content, BAD_CAST "HTTP/1.1 200 OK") != 0) {
                // debug
                propstat = NULL;
                prop = NULL;
                status = NULL;
              }
            } else {
              break;
            }
          }
          condition_throw(href && propstat && prop && status) UnspecifiedException("response unexpected");

          struct stat st = {
            .st_uid = server->options->uid,
            .st_gid = server->options->gid,
            .st_mode = (S_IFREG | 0777) & ~server->options->fmask,
            .st_nlink = 1
          };

          foreach(Node) (prop_entry, prop) {
            if (xmlStrcmp(prop_entry->ns->href, BAD_CAST "DAV:") != 0) {
              continue;
            }

            if (xmlStrcmp(prop_entry->name, BAD_CAST "getcontentlength") == 0) {
              st.st_size = strtol((const char *) prop_entry->children->content, NULL, 10);
            } else if (xmlStrcmp(prop_entry->name, BAD_CAST "resourcetype") == 0) {
              if (prop_entry->children && prop_entry->children->name &&
                  xmlStrcmp(prop_entry->children->name, BAD_CAST "collection") == 0) {
                st.st_mode = (S_IFDIR | 0777) & ~server->options->dmask;
                st.st_nlink = 2;
              }
            } else if (xmlStrcmp(prop_entry->name, BAD_CAST "creationdate") == 0) {
              st.st_ctime = xmlParserTimeNode(prop_entry);
            } else if (xmlStrcmp(prop_entry->name, BAD_CAST "getlastmodified") == 0) {
              st.st_mtime = xmlParserTimeNode(prop_entry);
            }
          }

          foreach(Node) (prop_entry, prop) {
            if (xmlStrcmp(prop_entry->ns->href, BAD_CAST NETWORKFS_XML_NS) != 0) {
              continue;
            }

            if (xmlStrcmp(prop_entry->name, BAD_CAST "mode") == 0) {
              char *rdev;
              st.st_mode = strtol((const char *) prop_entry->children->content, &rdev, 8);
              if (S_ISCHR(st.st_mode) || S_ISBLK(st.st_mode)) {
                st.st_rdev = strtol(rdev, NULL, 10);
              }
            } else if (xmlStrcmp(prop_entry->name, BAD_CAST "size") == 0) {
              st.st_size = strtol((const char *) prop_entry->children->content, NULL, 10);
            } else if (xmlStrcmp(prop_entry->name, BAD_CAST "owner") == 0) {
              char *gid;
              st.st_uid = strtol((const char *) prop_entry->children->content, &gid, 10);
              st.st_gid = strtol(gid, NULL, 10);
            } else if (xmlStrcmp(prop_entry->name, BAD_CAST "time") == 0) {
              char *end;
              st.st_ctime = strtol((const char *) prop_entry->children->content, &end, 10);
              st.st_mtime = strtol(end, &end, 10);
              st.st_atime = strtol(end, NULL, 10);
            }
          }

          const char *encoded_fullurl = (const char *) href->children->content;
          if (strscmp(encoded_fullurl, encoded_baseurl) != 0) {
            //debug
            printf("missing: %s, want %s\n", encoded_fullurl, encoded_baseurl);
            continue;
          }

          throwable with (auto filename = curl_easy_unescape_e(NULL, encoded_fullurl + strlen(encoded_baseurl), 0, NULL),
                          curl_free(filename)) {
            auto dirslash = strchr(filename, '/');
            if (dirslash) {
              *dirslash = '\0';
            }
            filler(buf, filename[0] == '\0' ? "." : filename, &st, 0, 0);
          }
        }
      }
    }
#if 1
  }
#endif

  return TEST_SUCCESS;
}


int dav_proppatch (struct DavServer *server, const char *path, const char *key, const char *value) {
  const char proppatch_body_template[] = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
  "<D:propertyupdate xmlns:D=\"DAV:\" xmlns:N=\"" NETWORKFS_XML_NS "\">\r\n"
    "<D:%s>\r\n"           //     set                remove
      "<D:prop>"  // no newline here!
        "<%s>%s</%s>\r\n"  // <N:key>value</N:key>  <N:key/>
      "</D:prop>\r\n"
    "</D:%s>\r\n"
  "</D:propertyupdate>\r\n";

  char proppatch_body[
    sizeof(proppatch_body_template) - 2 * 5 + strlen("remove") * 2 +
    strlen(key) * 2 + (value ? strlen(value) : 0)];
  snprintf(
    proppatch_body, sizeof(proppatch_body), proppatch_body_template,
    value ? "set" : "remove", key, value ? value : "", key, value ? "set" : "remove"
  );
  with (Buffer buf, Buffer(&buf, (char *) proppatch_body, strlen(proppatch_body), false), Buffer_destory(&buf)) {
    throwable with_curl (curl, server->baseuh, path, server->options) {
      throwable scope (curl_slist, list, CONTENT_TYPE_XML) {
        list = curl_slist_append_weak(list, PREFER_MINIMAL);
        curl_easy_setopt_or_die(curl, CURLOPT_HTTPHEADER, list);
        curl_easy_setopt_or_die(curl, CURLOPT_UPLOAD, 1L);
        curl_easy_setopt_or_die(curl, CURLOPT_READDATA, &buf);
        curl_easy_setopt_or_die(curl, CURLOPT_READFUNCTION, Buffer_fetch);
        curl_easy_setopt_or_die(curl, CURLOPT_INFILESIZE, (long) buf.size);
        curl_easy_setopt_or_die(curl, CURLOPT_CUSTOMREQUEST, "PROPPATCH");
        curl_easy_perform_or_die(curl);
      }
    }
  }
  return TEST_SUCCESS;
}


static size_t __dav_lock_callback (char *buffer, size_t size, size_t nitems, void *userdata) {
  SimpleString **res_p = (SimpleString **) userdata;

  if (*res_p == NULL) {
    if (strscmp(buffer, "Lock-Token:") == 0) {
      strnrstrip(buffer, nitems);
      buffer += strlen("Lock-Token:");
      buffer = strlstrip(buffer);
      // lighttpd bug
      if likely (buffer[0] == '<') {
        buffer++;
        buffer[strlen(buffer) - 1] = '\0';
      }
      *res_p = new(SimpleString) (buffer);
    }
  }

  return nitems;
}


static SimpleString *__dav_lock (struct DavServer *server, const char *path) {
  const char lock_body[] = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
  "<D:lockinfo xmlns:D=\"DAV:\">\r\n"
    "<D:lockscope><D:exclusive/></D:lockscope>\r\n"
    "<D:locktype><D:write/></D:locktype>\r\n"
    "<D:owner>\r\n"
      "<D:href>" NETWORKFS_URL "</D:href>\r\n"
    "</D:owner>\r\n"
  "</D:lockinfo>\r\n";

  SimpleString *token = NULL;

  try {
    with (Buffer buf, Buffer(&buf, (char *) lock_body, strlen(lock_body), false), Buffer_destory(&buf)) {
    #if 1
      throwable with (xmlParserCtxtPtr ctxt = NULL,
                      if (ctxt) xmlFreeDoc(ctxt->myDoc); xmlFreeParserCtxt(ctxt)) {
    #endif
        throwable with_curl (curl, server->baseuh, path, server->options) {
          //throwable scope (curl_slist, list, "Timeout: Infinite, Second-4100000000") {
          throwable scope (curl_slist, list, "Timeout: Second-600") {
            list = curl_slist_append_weak(list, CONTENT_TYPE_XML);
            curl_easy_setopt_or_die(curl, CURLOPT_HTTPHEADER, list);
            curl_easy_setopt_or_die(curl, CURLOPT_UPLOAD, 1L);
            curl_easy_setopt_or_die(curl, CURLOPT_HEADERDATA, &token);
            curl_easy_setopt_or_die(curl, CURLOPT_HEADERFUNCTION, __dav_lock_callback);
          #if 1
            curl_easy_setopt_or_die(curl, CURLOPT_WRITEDATA, &ctxt);
            curl_easy_setopt_or_die(curl, CURLOPT_WRITEFUNCTION, curl_parse_xml);
          #else
            curl_easy_setopt_or_die(curl, CURLOPT_NOBODY, 1L);
          #endif
            curl_easy_setopt_or_die(curl, CURLOPT_READDATA, &buf);
            curl_easy_setopt_or_die(curl, CURLOPT_READFUNCTION, Buffer_fetch);
            curl_easy_setopt_or_die(curl, CURLOPT_INFILESIZE, (long) buf.size);
            curl_easy_setopt_or_die(curl, CURLOPT_CUSTOMREQUEST, "LOCK");
            curl_easy_perform_or_die(curl);
          }
        }

        condition_throw(token) UnspecifiedException("LOCK doesn't return a token");

      #if 1
        xmlParseChunk(ctxt, NULL, 0, 1);

        auto doc = ctxt->myDoc;
        condition_throw(ctxt->wellFormed) UnspecifiedException("Failed to parse");

        foreach(Node) (lockdiscovery, xmlDocGetRootElement(doc)) {
          if (xmlStrcmp(lockdiscovery->name, BAD_CAST "lockdiscovery") != 0) {
            continue;
          }

          xmlNodePtr activelock = NULL;
          xmlNodePtr timeout = NULL;
          xmlNodePtr locktoken = NULL;
          xmlNodePtr href = NULL;
          foreach(Node) (lockdiscovery_node, lockdiscovery) {
            if (xmlStrcmp(lockdiscovery_node->name, BAD_CAST "activelock") == 0) {
              activelock = lockdiscovery_node;
              foreach(Node) (activelock_node, activelock) {
                if (timeout == NULL) {
                  if (xmlStrcmp(activelock_node->name, BAD_CAST "timeout") == 0) {
                    timeout = activelock_node;
                  }
                } else if (locktoken == NULL) {
                  if (xmlStrcmp(activelock_node->name, BAD_CAST "locktoken") == 0) {
                    locktoken = activelock_node;
                    foreach(Node) (locktoken_node, locktoken) {
                      if (xmlStrcmp(locktoken_node->name, BAD_CAST "href") == 0) {
                        href = locktoken_node;
                        break;
                      }
                    }
                  }
                } else {
                  break;
                }
              }
            }
            break;
          }
          condition_throw(
            activelock && timeout && locktoken && href
          ) UnspecifiedException("prop unexpected");

          if (xmlStrscmp(href->children->content, BAD_CAST token->str) != 0) {
            throw UnspecifiedException("lock href mismatched");
          }
        }
      #endif
    #if 1
      }
    #endif
    }
  } onerror (e) {
    delete(SimpleString) token;
    token = NULL;
  }

  return token;
}


static int __dav_unlock (struct DavServer *server, const char *path, SimpleString *token) {
  with_curl (curl, server->baseuh, path, server->options) {
    char token_header[sizeof("Lock-Token: <>") + token->len];
    snprintf(token_header, sizeof(token_header), "Lock-Token: <%s>", token->str);
    throwable scope (curl_slist, list, token_header) {
      curl_easy_setopt_or_die(curl, CURLOPT_HTTPHEADER, list);
      curl_easy_setopt_or_die(curl, CURLOPT_CUSTOMREQUEST, "UNLOCK");
      curl_easy_perform_or_die(curl);
    }
  }
  return TEST_SUCCESS;
}


int dav_lock (struct DavServer *server, const char *path) {
  if (!server->options->use_lock) {
    return 0;
  }

  struct FileLock *filelock = NULL;
  synchronized (rwlock, &server->filelock_tree_lock, rdlock) {
    struct FileLock **filelock_p = tfind(&path, &server->filelock_tree, __dav_strpcmp);
    if (filelock_p) {
      filelock = *filelock_p;
      filelock->cnt++;
    }
  }
  if (filelock) {
    return 0;
  }

  try {
    throwable filelock = malloc_et(struct FileLock);
    try {
      filelock->path = NULL;
      filelock->token = NULL;
      filelock->cnt = 1;
      filelock->path = strdup(path);
      condition_throw(filelock->path) MallocException("path dup");
      throwable synchronized (rwlock, &server->filelock_tree_lock, wrlock) {
        struct FileLock **exist_filelock_p = tfind(&path, &server->filelock_tree, __dav_strpcmp);
        if (exist_filelock_p) {
          struct FileLock *exist_filelock = *exist_filelock_p;
          exist_filelock->cnt++;
          free(filelock->path);
          filelock->path = NULL;
          free(filelock);
          filelock = NULL;
          break;
        }
        throwable filelock->token = __dav_lock(server, path);
        struct FileLock **filelock_p = tsearch(filelock, &server->filelock_tree, __dav_strpcmp);
        condition_throw(filelock_p)
          MallocException("filelock table insert");
      }
    } onerror (e) {
      free(filelock->path);
      if (filelock->token) {
        __dav_unlock(server, path, filelock->token);
      }
    }
  } onerror (e) {
    free(filelock);
  }

  return TEST_SUCCESS;
}


int dav_unlock (struct DavServer *server, const char *path) {
  if (!server->options->use_lock) {
    return 0;
  }

  int res = 0;

  struct FileLock *filelock = NULL;
  synchronized (rwlock, &server->filelock_tree_lock, rdlock) {
    struct FileLock **filelock_p = tfind(&path, &server->filelock_tree, __dav_strpcmp);
    if likely (filelock_p) {
      filelock = *filelock_p;
    }
    should (filelock) otherwise {
      UnspecifiedException("release cannot find file lock");
      res = 1;
      break;
    }
    auto cnt_remain = --filelock->cnt;
    if (cnt_remain > 0) {
      filelock = NULL;
    }
  }
  if (filelock == NULL) {
    return res;
  }

  synchronized (rwlock, &server->filelock_tree_lock, wrlock) {
    struct FileLock **filelock_p = tfind(&path, &server->filelock_tree, __dav_strpcmp);
    if (filelock_p == NULL) {
      break;
    }
    if ((*filelock_p)->cnt <= 0) {
      filelock = *filelock_p;
      tdelete(filelock, &server->filelock_tree, __dav_strpcmp);
    }
  }
  if (filelock == NULL) {
    return 0;
  }

  res = __dav_unlock(server, filelock->path, filelock->token);
  delete(FileLock) filelock;
  return res;
}


static size_t __dav_options_callback (char *buffer, size_t size, size_t nitems, void *userdata) {
  struct DavServer *server = (struct DavServer *) userdata;

  if (strscmp(buffer, "Allow:") == 0) {
    strnrstrip(buffer, nitems - 2);
    buffer += strlen("Allow:");
    buffer = strlstrip(buffer);
    foreach(strtok) (option, buffer, ", ") {
      if (0);
    #define X(o) else if (strcmp(option, # o) == 0) server->o = true;
      DAV_METHOD
    #undef X
    }
  } else if (strscmp(buffer, "Server:") == 0) {
    strnrstrip(buffer, nitems - 2);
    buffer += strlen("Server:");
    buffer = strlstrip(buffer);
    char *version = strchr(buffer, '/');
    *version = '\0';
    version++;
    server->server = strdup(buffer);
    server->version = strdup(version);
  }

  return nitems;
}


int dav_options (struct DavServer *server) {
  try {
    throwable with_curl (curl, server->baseuh, NULL, server->options) {
      if (server->options->initial_timeout) {
        curl_easy_setopt_or_die(curl, CURLOPT_TIMEOUT, server->options->initial_timeout);
      }
      curl_easy_setopt_or_die(curl, CURLOPT_NOBODY, 1L);

      /* handle redirect */
      curl_easy_setopt_or_die(curl, CURLOPT_FOLLOWLOCATION, 0L);
      curl_easy_perform_or_die(curl);

      long response_code;
      curl_easy_getinfo_or_die(curl, CURLINFO_RESPONSE_CODE, &response_code);

      if (response_code / 100 == 3) {
        /* a redirect implies a 3xx response code */
        curl_char *baseurl = NULL;
        curl_easy_getinfo_or_die(curl, CURLINFO_REDIRECT_URL, &baseurl);
        if (baseurl) {
          printf("Redirected to: %s\n", baseurl);
          curl_url_set_or_die(server->baseuh, CURLUPART_URL, baseurl, 0);
        }
      }

      curl_easy_setopt_or_die(curl, CURLOPT_FOLLOWLOCATION, 1L);

      /* get options */
    //#if CURL_AT_LEAST_VERSION(7, 63, 0)
      //curl_easy_setopt_or_die(curl, CURLOPT_CURLU, server->baseuh);
    //#else
      throwable with (curl_char *s_url = NULL,
                      curl_url_get_or_die(server->baseuh, CURLUPART_URL, &s_url, 0),
                      curl_free(s_url)) {
        curl_easy_setopt_or_die(curl, CURLOPT_URL, s_url);
      }
    //#endif

      curl_easy_setopt_or_die(curl, CURLOPT_CUSTOMREQUEST, "OPTIONS");
      curl_easy_setopt_or_die(curl, CURLOPT_HEADERDATA, server);
      curl_easy_setopt_or_die(curl, CURLOPT_HEADERFUNCTION, __dav_options_callback);
      curl_easy_perform_or_die(curl);
    }

    if unlikely (server->LOCK != server->UNLOCK) {
      printf("The server does not support both LOCK and UNLOCK!\n");
      server->LOCK = false;
      server->UNLOCK = false;
    }

    if (server->options->use_lock) {
      server->filelock_tree = NULL;
      pthread_rwlock_init(&server->filelock_tree_lock, NULL);
    }
  } onerror (e) {}

  return TEST_SUCCESS;
}


static struct DavServer *__dav_unlock_node_server;


static void __dav_unlock_node (void *nodep) {
  struct FileLock *filelock = (struct FileLock *) nodep;
  __dav_unlock(__dav_unlock_node_server, filelock->path, filelock->token);
  delete(FileLock) filelock;
}


void dav_destory (struct DavServer *server) {
  if (server->options->use_lock) {
    __dav_unlock_node_server = server;
    tdestroy(server->filelock_tree, __dav_unlock_node);
    pthread_rwlock_destroy(&server->filelock_tree_lock);
  }
  delete(CURLU) server->baseuh;
}
