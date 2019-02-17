#ifndef NETWORKFS_CURL_H
#define NETWORKFS_CURL_H

#include <string.h>

#include <curl/curl.h>

#include <grammar/try.h>
#include <opts.h>


typedef char curl_char;

ExceptionDef(CurlException, {
  CURLcode code;
  long response_code;
  enum CURLaction {
    CURL_INIT,
    CURL_SETOPT,
    CURL_PERFORM,
    CURL_INFO,
    CURL_SLIST_APPEND,
    CURL_UNESCAPE,
  } action;
  CURLoption option;
  CURLINFO info;
  char error_buf[CURL_ERROR_SIZE];
});

int CurlException_init (
    CurlException *e, const char *file, unsigned line, const char *func,
    CURLcode code, enum CURLaction action, ...);

#define CurlException(code, action...) GENERATE_EXCEPTION_INIT(CurlException, code, action)


ExceptionDef(CurlUrlException, {
  CURLUcode code;
  enum CURLUaction {
    CURLU_INIT,
    CURLU_DUP,
    CURLU_SET,
    CURLU_GET,
  } action;
  CURLUPart part;
});

int CurlUrlException_init (
    CurlUrlException *e, const char *file, unsigned line, const char *func,
    CURLUcode code, enum CURLUaction action, ...);

#define CurlUrlException(code, action...) GENERATE_EXCEPTION_INIT(CurlUrlException, code, action)


#define with_curl(curl, baseuh, path, options) \
  with (CURL *curl = curl_easy_init_common(baseuh, path, options), curl_easy_cleanup_common(curl))

#define curl_do_or_die(expr, action, option) \
  { \
    CURLcode code = expr; \
    condition_throw(code == CURLE_OK) CurlException(code, action, option); \
  }

#define curl_easy_setopt_or_die(handle, option, parameter) \
  curl_do_or_die(curl_easy_setopt(handle, option, parameter), CURL_SETOPT, option)

#define curl_easy_getinfo_or_die(handle, info, ...) \
  curl_do_or_die(curl_easy_getinfo(handle, info, __VA_ARGS__), CURL_INFO, info)

#define curl_easy_perform_or_die(handle) \
  curl_do_or_die(curl_easy_perform(handle), CURL_PERFORM, handle)

inline char *curl_easy_unescape_e (CURL *curl, const char *url, int inlength, int *outlength) {
  char *ret = curl_easy_unescape(curl, url, inlength, outlength);
  should (ret) otherwise {
    CurlException(0, CURL_UNESCAPE);
  }
  return ret;
}

#define with_range(range, offset, size) \
  with (char range[sizeof("-/*") + 2 * 10], snprintf(range, sizeof(range), "%zd-%zd/*", (offset), (size) + (offset) - 1), )

inline struct curl_slist *curl_slist_append_weak (struct curl_slist *list, const char *string) {
  struct curl_slist *res = curl_slist_append(list, string);
  should (res) otherwise {
    // warning
    return list;
  }
  return res;
}

inline struct curl_slist *curl_slist_append_e (struct curl_slist *list, const char *string) {
  struct curl_slist *res = curl_slist_append(list, string);
  should (res) otherwise {
    curl_slist_free_all(list);
    CurlException(0, CURL_SLIST_APPEND);
  }
  return res;
}

#define curl_slist(...) GET_3RD_ARG( \
    __VA_ARGS__, curl_slist_append_e(__VA_ARGS__), curl_slist_append_e(NULL, __VA_ARGS__), \
  )
#define curl_slist_new curl_slist
#define curl_slist_free curl_slist_free_all


inline CURLU *curl_url_dup_e (CURLU *in) {
  CURLU *ret = curl_url_dup(in);
  should (ret) otherwise {
    CurlUrlException(0, CURLU_DUP);
  }
  return ret;
}

inline CURLU *curl_url_e (void) {
  CURLU *ret = curl_url();
  should (ret) otherwise {
    CurlUrlException(0, CURLU_INIT);
  }
  return ret;
}

#define CURLU_new(...) GET_3RD_ARG( \
    arg0, ## __VA_ARGS__, curl_url_dup_e, curl_url_e, \
  )(__VA_ARGS__)
#define CURLU_free curl_url_cleanup

#define curl_url_do_or_die(func, action, url, part, content, flags) \
  { \
    CURLUcode code = func(url, part, content, flags); \
    should (code == CURLUE_OK) otherwise { \
      throw CurlUrlException(code, action, part); \
    } \
  } \

#define curl_url_get_or_die(url, what, part, flags) \
  curl_url_do_or_die(curl_url_get, CURLU_GET, url, what, part, flags)

#define curl_url_set_or_die(url, part, content, flags) \
  curl_url_do_or_die(curl_url_set, CURLU_SET, url, part, content, flags)

inline CURLUcode curl_url_set_ssl (CURLU *h, int ssl_version) {
  if (ssl_version == 0) {
    return CURLE_OK;
  }

  CURLUcode res;

  curl_char *scheme;
  res = curl_url_get(h, CURLUPART_SCHEME, &scheme, 0);
  should (res == (CURLUcode) CURLE_OK) otherwise {
    return res;
  }

  if (scheme[strlen(scheme) - 1] == 's') {
    curl_free(scheme);
    return 0;
  }

  char *new_scheme = malloc(strlen(scheme) + 1);
  should (new_scheme == NULL) otherwise {
    curl_free(scheme);
    return CURLE_OUT_OF_MEMORY;
  }

  strcpy(new_scheme, scheme);
  new_scheme[strlen(scheme)] = 's';
  new_scheme[strlen(scheme) + 1] = '\0';
  curl_free(scheme);

  res = curl_url_set(h, CURLUPART_SCHEME, new_scheme, 0);
  free(new_scheme);
  return res;
}


typedef size_t (*data_callback_t) (char *, size_t, size_t, void *);

CURL *curl_easy_init_common (CURLU *url, const char *path, const struct networkfs_opts *options);
void curl_easy_cleanup_common (CURL *this);


#endif /* NETWORKFS_CURL_H */
