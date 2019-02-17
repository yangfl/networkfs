#ifndef NETWORKFS_OPTS_H
#define NETWORKFS_OPTS_H

#include <stdbool.h>
#include <sys/stat.h>

#include <curl/curl.h>


struct networkfs_opts {
  CURLU *baseurl;
  char *username;
  char *password;
  long use_ssl;
  long ssl_version;
  long ip_version;

  uid_t uid;
  gid_t gid;
  mode_t fmask;
  mode_t dmask;

  double dir_timeout;

  char *interface;
  long timeout;
  long connect_timeout;
  long initial_timeout;

  bool no_verify_hostname;
  bool no_verify_peer;
  char* cert;
  char* cert_type;
  char* key;
  char* key_type;
  char* key_password;
  char* engine;
  char* cacert;
  char* capath;
  char* ciphers;

  char* proxy;
  char* proxy_userpwd;
  long proxytype;
  long proxyauth;
  bool proxytunnel;

  bool use_lock;
};


#endif /* NETWORKFS_OPTS_H */
