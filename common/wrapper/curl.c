#include <stdarg.h>

#include <grammar/class.h>
#include <template/stack.h>
#include "../networkfs.h"

#include "curl.h"


extern inline char *curl_easy_unescape_e (CURL *curl, const char *url, int inlength, int *outlength);
extern inline struct curl_slist *curl_slist_append_weak (struct curl_slist *list, const char *string);
extern inline struct curl_slist *curl_slist_append_e (struct curl_slist *list, const char *string);
extern inline CURLU *curl_url_dup_e (CURLU *in);
extern inline CURLU *curl_url_e (void);
extern inline CURLUcode curl_url_set_ssl (CURLU *h, int ssl_version);


static Stack curl_handler_stack;


static void __attribute__((constructor)) curl_load (void) {
  Stack_init(&curl_handler_stack);
}


int CurlException_init (
    CurlException *e, const char *file, unsigned line, const char *func,
    CURLcode code, enum CURLaction action, ...) {
  bool no_bt = false;

  e->code = code;
  e->action = action;
  e->VTABLE(Exception) = &VTABLE_OF(Exception, CurlException);

  va_list args;
  va_start(args, action);

  switch (e->action) {
    case CURL_SETOPT:
      e->option = va_arg(args, typeof(e->option));
      break;
    case CURL_INFO:
      e->info = va_arg(args, typeof(e->info));
      break;
    case CURL_PERFORM:
      if (e->code == CURLE_HTTP_RETURNED_ERROR) {
        curl_easy_getinfo(va_arg(args, typeof(CURL *)), CURLINFO_RESPONSE_CODE, &e->response_code);
        no_bt = true;
      }
      break;
    default:
      break;
  }

  va_end(args);

  return Exception_init((Exception *) e, file, line, func, no_bt);
}


static int Exception_fputs_curl (const BaseException *e_, FILE *stream) {
  const CurlException *e = (const CurlException *) e_;
  int res = 0;

  switch (e->action) {
    case CURL_INIT:
      return fprintf(stream, "Error init curl\n");
    case CURL_SLIST_APPEND:
      return fprintf(stream, "Error appending to curl list\n");
    case CURL_UNESCAPE:
      return fprintf(stream, "Error esacpe curl URL\n");
    case CURL_SETOPT:
      res += fprintf(stream, "Error setting curl %d: ", e->option);
      break;
    case CURL_PERFORM:
      res += fprintf(stream, "Error performing curl: ");
      break;
    case CURL_INFO:
      res += fprintf(stream, "Error getting curl info %d: ", e->info);
      break;
  }

  res += fprintf(
    stream, "%s\n",
    e->error_buf[0] == '\0' ? curl_easy_strerror(e->code) : e->error_buf
  );

  return res;
}


VTABLE_INIT(Exception, CurlException) = {
  .name = "CurlException",
  .destory = NULL,
  .fputs = Exception_fputs_curl
};


int CurlUrlException_init (
    CurlUrlException *e, const char *file, unsigned line, const char *func,
    CURLUcode code, enum CURLUaction action, ...) {
  e->code = code;
  e->action = action;
  e->VTABLE(Exception) = &VTABLE_OF(Exception, CurlUrlException);

  if (e->action == CURLU_SET || e->action == CURLU_GET) {
    va_list args;
    va_start(args, action);
    e->part = va_arg(args, typeof(e->part));
    va_end(args);
  }

  return Exception_init((Exception *) e, file, line, func, 0);
}


static int Exception_fputs_curlurl (const BaseException *e_, FILE *stream) {
  const CurlUrlException *e = (const CurlUrlException *) e_;

  const char *s_action;
  switch (e->action) {
    case CURLU_INIT:
      return fprintf(stream, "Error init URL\n");
    case CURLU_DUP:
      return fprintf(stream, "Error duplicate URL\n");
    case CURLU_SET:
      s_action = "getting";
      break;
    case CURLU_GET:
      s_action = "setting";
      break;
    default:
      s_action = "(Unknown action)";
  }

  const char *s_part;
  switch (e->part) {
    case CURLUPART_URL:
      s_part = "full url";
      break;
    case CURLUPART_SCHEME:
      s_part = "scheme";
      break;
    case CURLUPART_USER:
      s_part = "user";
      break;
    case CURLUPART_PASSWORD:
      s_part = "password";
      break;
    case CURLUPART_OPTIONS:
      s_part = "options";
      break;
    case CURLUPART_HOST:
      s_part = "host";
      break;
    case CURLUPART_PORT:
      s_part = "port";
      break;
    case CURLUPART_PATH:
      s_part = "path";
      break;
    case CURLUPART_QUERY:
      s_part = "query";
      break;
    case CURLUPART_FRAGMENT:
      s_part = "fragment";
      break;
    default:
      s_part = "(Unknown part)";
  }

  const char *msg;
  switch (e->code) {
    case CURLUE_OK:
      msg = "OK";
      break;
    case CURLUE_BAD_HANDLE:
      msg = "Bad handle";
      break;
    case CURLUE_BAD_PARTPOINTER:
      msg = "Bad partpointer";
      break;
    case CURLUE_MALFORMED_INPUT:
      msg = "Malformed input";
      break;
    case CURLUE_BAD_PORT_NUMBER:
      msg = "Bad port number";
      break;
    case CURLUE_UNSUPPORTED_SCHEME:
      msg = "Unsupported scheme";
      break;
    case CURLUE_URLDECODE:
      msg = "URL decode";
      break;
    case CURLUE_OUT_OF_MEMORY:
      msg = "Out of memory";
      break;
    case CURLUE_USER_NOT_ALLOWED:
      msg = "User not allowed";
      break;
    case CURLUE_UNKNOWN_PART:
      msg = "Unknown part";
      break;
    case CURLUE_NO_SCHEME:
      msg = "No scheme";
      break;
    case CURLUE_NO_USER:
      msg = "No user";
      break;
    case CURLUE_NO_PASSWORD:
      msg = "No password";
      break;
    case CURLUE_NO_OPTIONS:
      msg = "No options";
      break;
    case CURLUE_NO_HOST:
      msg = "No host";
      break;
    case CURLUE_NO_PORT:
      msg = "No port";
      break;
    case CURLUE_NO_QUERY:
      msg = "No query";
      break;
    case CURLUE_NO_FRAGMENT:
      msg = "No fragment";
      break;
    default:
      msg = "Unknown error code";
  }

  return fprintf(
    stream, "Error %s URL %s: %s\n", s_action, s_part, msg
  );
}


VTABLE_INIT(Exception, CurlUrlException) = {
  .name = "CurlUrlException",
  .destory = NULL,
  .fputs = Exception_fputs_curlurl
};


CURL *curl_easy_init_common (
    CURLU *url, const char *path, const struct networkfs_opts *options) {
  CURL *this;

  try {
    LinkedList *node = (LinkedList *) Stack_pop(&curl_handler_stack);
    if (node) {
      this = node->value;
      free(node);
      curl_easy_reset(this);
    } else {
      this = curl_easy_init();
      condition_throw(this) CurlException(0, CURL_INIT);
    }

    ((CurlException *) &ex)->error_buf[0] = '\0';
    curl_easy_setopt_or_die(this, CURLOPT_ERRORBUFFER, ((CurlException *) &ex)->error_buf);

    throwable scope (CURLU, clean_url, url) {
      if likely (path) {
        if likely (path[0] == '/') {
          path++;
        }
        if likely (path[0] != '\0') {
          curl_url_set_or_die(clean_url, CURLUPART_URL, path, CURLU_URLENCODE);
        }
      }

      throwable with (curl_char *s_url = NULL,
                      curl_url_get_or_die(clean_url, CURLUPART_URL, &s_url, 0),
                      curl_free(s_url)) {
        curl_easy_setopt_or_die(this, CURLOPT_URL, s_url);
      }
    }

    if likely (options) {
      curl_easy_setopt_or_die(this, CURLOPT_USERNAME, options->username);
      curl_easy_setopt_or_die(this, CURLOPT_PASSWORD, options->password);

      if (options->interface) {
        curl_easy_setopt_or_die(this, CURLOPT_INTERFACE, options->interface);
      }

      if (options->ip_version) {
        curl_easy_setopt_or_die(this, CURLOPT_IPRESOLVE, options->ip_version);
      }

      if (options->proxy) {
        curl_easy_setopt_or_die(this, CURLOPT_PROXY, options->proxy);
        if (options->proxytype) {
          curl_easy_setopt_or_die(this, CURLOPT_PROXYTYPE, options->proxytype);
        }
        if (options->proxyauth) {
          curl_easy_setopt_or_die(this, CURLOPT_PROXYAUTH, options->proxyauth);
        }
        if (options->proxytunnel) {
          curl_easy_setopt_or_die(this, CURLOPT_HTTPPROXYTUNNEL, 1L);
        }
        curl_easy_setopt_or_die(this, CURLOPT_PROXYUSERPWD, options->proxy_userpwd);
      }

      if (options->use_ssl) {
        curl_easy_setopt_or_die(this, CURLOPT_USE_SSL, options->use_ssl);
        if (options->ssl_version) {
          curl_easy_setopt_or_die(this, CURLOPT_SSLVERSION, options->ssl_version);
        }
        if (options->no_verify_hostname) {
          /* The default is 2 which verifies even the host string. This sets to 1
           * which means verify the host but not the string. */
          curl_easy_setopt_or_die(this, CURLOPT_SSL_VERIFYHOST, 1L);
        }
        if (options->no_verify_peer) {
          curl_easy_setopt_or_die(this, CURLOPT_SSL_VERIFYPEER, 0L);
        }
        if (options->cacert) {
          curl_easy_setopt_or_die(this, CURLOPT_CAINFO, options->cacert);
        }
        if (options->capath) {
          curl_easy_setopt_or_die(this, CURLOPT_CAPATH, options->capath);
        }
        if (options->ciphers) {
          curl_easy_setopt_or_die(this, CURLOPT_SSL_CIPHER_LIST, options->ciphers);
        }
        curl_easy_setopt_or_die(this, CURLOPT_SSLCERT, options->cert);
        curl_easy_setopt_or_die(this, CURLOPT_SSLCERTTYPE, options->cert_type);
        curl_easy_setopt_or_die(this, CURLOPT_SSLKEY, options->key);
        curl_easy_setopt_or_die(this, CURLOPT_SSLKEYTYPE, options->key_type);
        curl_easy_setopt_or_die(this, CURLOPT_SSLKEYPASSWD, options->key_password);
      }

      if (options->timeout) {
        curl_easy_setopt_or_die(this, CURLOPT_TIMEOUT, options->timeout);
      }
      if (options->connect_timeout) {
        curl_easy_setopt_or_die(this, CURLOPT_CONNECTTIMEOUT, options->connect_timeout);
      }
    }
    curl_easy_setopt_or_die(this, CURLOPT_USERAGENT, NETWORKFS_NAME "/" NETWORKFS_VERSION);
    curl_easy_setopt_or_die(this, CURLOPT_FAILONERROR, 1L);
    curl_easy_setopt_or_die(this, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt_or_die(this, CURLOPT_NETRC, CURL_NETRC_OPTIONAL);
  } onerror (e) {
    curl_easy_cleanup(this);
    this = NULL;
  }

  return this;
}


void curl_easy_cleanup_common (CURL *this) {
  if (this) {
    hideE try {
      auto node = malloc_et(LinkedList);
      check;
      node->value = this;
      Stack_push(&curl_handler_stack, (LinkedListHead *) node);
    } catch (e) {
      Exception_fputs(e, stderr);
      curl_easy_cleanup(this);
    }
  }
}
