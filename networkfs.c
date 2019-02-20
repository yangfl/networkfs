#define _GNU_SOURCE

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <float.h>
#include <stddef.h>
#include <unistd.h>
#include <getopt.h>

#ifndef FUSE_USE_VERSION
#define FUSE_USE_VERSION 31
#endif

#include <fuse.h>
#include <curl/curl.h>

#if !CURL_AT_LEAST_VERSION(7, 62, 0)
#error "this program requires curl 7.62.0 or later"
#endif

#include <grammar/class.h>
#include <wrapper/curl.h>
#include <wrapper/log.h>
#include <opts.h>

#include "proto/proto.h"
#include "emulator/emulator.h"
#include "networkfs.h"


enum {
  KEY_VERSION,
};


struct _networkfs_opts {
  struct networkfs_opts;

  char *scheme;
  char *port;

  bool proxyanyauth;
  bool proxybasic;
  bool proxydigest;
  bool proxyntlm;

  bool uid_set;
  bool gid_set;
  bool set_uid;
  bool set_gid;
  bool entry_timeout_set;
  bool negative_timeout_set;
  bool attr_timeout_set;
  bool hide_password;
  bool show_help;
} options;

#define NETWORKFS_OPT(t, p, v) {t, offsetof(struct _networkfs_opts, p), v}
#define NETWORKFS_OPT_KEY(t, p) NETWORKFS_OPT(t, p, 1)

static const struct fuse_opt option_spec[] = {
  // -- url --
  NETWORKFS_OPT_KEY("scheme=%s",   scheme),
  NETWORKFS_OPT_KEY("port=%s",     port),
  NETWORKFS_OPT_KEY("user=%s",     username),
  NETWORKFS_OPT_KEY("username=%s", username),
  NETWORKFS_OPT_KEY("password=%s", password),

  // -- main --
  NETWORKFS_OPT("hide_password",    hide_password, true),
  NETWORKFS_OPT("no_hide_password", hide_password, false),
  NETWORKFS_OPT_KEY("use_lock",     use_lock),

  // -- fuse --
  NETWORKFS_OPT_KEY("fmask=%o", fmask),
  NETWORKFS_OPT_KEY("dmask=%o", dmask),

  NETWORKFS_OPT_KEY("uid=%d",            uid),
  NETWORKFS_OPT_KEY("gid=%d",            gid),
  NETWORKFS_OPT_KEY("set_uid",           set_uid),
  NETWORKFS_OPT_KEY("set_gid",           set_gid),
  NETWORKFS_OPT_KEY("entry_timeout=",    entry_timeout_set),
  NETWORKFS_OPT_KEY("negative_timeout=", negative_timeout_set),
  NETWORKFS_OPT_KEY("attr_timeout=",     attr_timeout_set),
  NETWORKFS_OPT_KEY("dir_timeout=%lf",   dir_timeout),

  // -- curl --
  NETWORKFS_OPT_KEY("interface=%s", interface),

  NETWORKFS_OPT("ipv4", ip_version, CURL_IPRESOLVE_V4),
  NETWORKFS_OPT("ipv6", ip_version, CURL_IPRESOLVE_V6),

  NETWORKFS_OPT_KEY("proxy=%s",      proxy),
  NETWORKFS_OPT_KEY("proxytunnel",   proxytunnel),
  NETWORKFS_OPT_KEY("proxy_anyauth", proxyanyauth),
  NETWORKFS_OPT_KEY("proxy_basic",   proxybasic),
  NETWORKFS_OPT_KEY("proxy_digest",  proxydigest),
  NETWORKFS_OPT_KEY("proxy_ntlm",    proxyntlm),
  NETWORKFS_OPT("httpproxy",         proxytype, CURLPROXY_HTTP),
  NETWORKFS_OPT("socks4",            proxytype, CURLPROXY_SOCKS4),
  NETWORKFS_OPT("socks5",            proxytype, CURLPROXY_SOCKS5),
  NETWORKFS_OPT_KEY("proxy_user=%s", proxy_userpwd),

  // use_ssl
  NETWORKFS_OPT("ssl",         use_ssl, CURLUSESSL_ALL),
  NETWORKFS_OPT("ssl_control", use_ssl, CURLUSESSL_CONTROL),
  NETWORKFS_OPT("ssl_try",     use_ssl, CURLUSESSL_TRY),

  NETWORKFS_OPT_KEY("timeout=%ld",         timeout),
  NETWORKFS_OPT_KEY("connect_timeout=%ld", connect_timeout),
  NETWORKFS_OPT_KEY("initial_timeout=%ld", initial_timeout),

  // ssl_version
  NETWORKFS_OPT("tlsv1.3", ssl_version, CURL_SSLVERSION_TLSv1_3),
  NETWORKFS_OPT("tlsv1.2", ssl_version, CURL_SSLVERSION_TLSv1_2),
  NETWORKFS_OPT("tlsv1.1", ssl_version, CURL_SSLVERSION_TLSv1_1),
  NETWORKFS_OPT("tlsv1.0", ssl_version, CURL_SSLVERSION_TLSv1_0),
  NETWORKFS_OPT("tlsv1",   ssl_version, CURL_SSLVERSION_TLSv1),
  NETWORKFS_OPT("sslv3",   ssl_version, CURL_SSLVERSION_SSLv3),

  NETWORKFS_OPT_KEY("no_verify_hostname", no_verify_hostname),
  NETWORKFS_OPT_KEY("no_verify_peer",     no_verify_peer),
  NETWORKFS_OPT_KEY("cert=%s",            cert),
  NETWORKFS_OPT_KEY("cert_type=%s",       cert_type),
  NETWORKFS_OPT_KEY("key=%s",             key),
  NETWORKFS_OPT_KEY("key_type=%s",        key_type),
  NETWORKFS_OPT_KEY("pass=%s",            key_password),
  NETWORKFS_OPT_KEY("engine=%s",          engine),
  NETWORKFS_OPT_KEY("cacert=%s",          cacert),
  NETWORKFS_OPT_KEY("capath=%s",          capath),
  NETWORKFS_OPT_KEY("ciphers=%s",         ciphers),

  NETWORKFS_OPT_KEY("-h",     show_help),
  NETWORKFS_OPT_KEY("--help", show_help),
  FUSE_OPT_KEY("-V",          KEY_VERSION),
  FUSE_OPT_KEY("--version",   KEY_VERSION),
  FUSE_OPT_END
};


static void usage (const char *progname) {
  printf("usage: %s [options] <url> <mountpoint>\n\n", progname);
  printf(
"NetworkFS options:\n"
// -- url --
"    -o scheme=STR          set/override server scheme\n"
"    -o port=STR            set/override server port\n"
"    -o user=STR            set/override server username\n"
"    -o username=STR        set/override server username\n"
"    -o password=STR        set/override server password\n"
// -- main --
"    -o (no_)hide_password  (do not) hide password from ps (yes)\n"
"    -o use_lock            use WebDAV lock\n"
// -- fuse --
"    -o set_uid             override existing uid\n"
"    -o set_gid             override existing gid\n"
"    -o fmask=M             set file permissions (octal, 0133)\n"
"    -o dmask=M             set file permissions (octal, 0022)\n"
"    -o entry_timeout=T     cache timeout for names (30.0s)\n"
"    -o negative_timeout=T  cache timeout for deleted names (10.0s)\n"
"    -o attr_timeout=T      cache timeout for attributes (30.0s)\n"
"    -o dir_timeout=T       cache timeout for dir list (10.0s)\n"
// -- curl --
"    -o interface=STR       specify network interface/address to use\n"
// ip_version
"    -o ipv4                resolve name to IPv4 address\n"
"    -o ipv6                resolve name to IPv6 address\n"
// proxy
"    -o proxy=STR           use host:port HTTP proxy\n"
"    -o proxytunnel         operate through a HTTP proxy tunnel (using CONNECT)\n"
"    -o proxy_anyauth       pick \"any\" proxy authentication method\n"
"    -o proxy_basic         use Basic authentication on the proxy\n"
"    -o proxy_digest        use Digest authentication on the proxy\n"
"    -o proxy_ntlm          use NTLM authentication on the proxy\n"
"    -o httpproxy           use a HTTP proxy (default)\n"
"    -o socks4              use a SOCKS4 proxy\n"
"    -o socks5              use a SOCKS5 proxy\n"
"    -o proxy_user=STR      set proxy user and password\n"
// use_ssl
"    -o ssl                 enable SSL/TLS for both control and data connections\n"
"    -o ssl_control         enable SSL/TLS only for control connection\n"
"    -o ssl_try             try SSL/TLS first but connect anyway\n"
// ssl_version
"    -o tlsv1.[0|1|2|3]     use TLSv1.[0|1|2|3] (SSL)\n"
"    -o tlsv1               use TLSv1 (SSL)\n"
"    -o sslv3               use SSLv3 (SSL)\n"
// ssl options
"    -o no_verify_hostname  does not verify the hostname (SSL)\n"
"    -o no_verify_peer      does not verify the peer (SSL)\n"
"    -o cert=STR            client certificate file (SSL)\n"
"    -o cert_type=STR       certificate file type (DER/PEM/ENG) (SSL)\n"
"    -o key=STR             private key file name (SSL)\n"
"    -o key_type=STR        private key file type (DER/PEM/ENG) (SSL)\n"
"    -o pass=STR            pass phrase for the private key (SSL)\n"
"    -o engine=STR          crypto engine to use (SSL)\n"
"    -o cacert=STR          file with CA certificates to verify the peer (SSL)\n"
"    -o capath=STR          CA directory to verify peer against (SSL)\n"
"    -o ciphers=STR         SSL ciphers to use (SSL)\n"
// timeout
"    -o timeout=T           maximum time allowed for operation in seconds\n"
"    -o connect_timeout=T   maximum time allowed for connection in seconds\n"
"    -o initial_timeout=T   maximum time allowed for the first connection in\n"
"                           seconds (5s)\n"
"\n");
}


static inline void options_set_default (void) {
  options.uid = getuid();
  options.gid = getgid();
  options.fmask = 0133;
  options.dmask = 0022;

  options.dir_timeout = 10;

  options.initial_timeout = 5;

  options.hide_password = true;
}


extern inline int NetworkFSException_init (
    NetworkFSException *e, const char *file, const char *func, unsigned line,
    const char *what);


VTABLE_INIT(Exception, NetworkFSException) = {
  .name = "NetworkFSException",
  .fputs = NULL,
  .destory = Exception_destory_dynamic
};


static inline int combine_url (CURLU *h, struct _networkfs_opts *options) {
  int ret = 0;

  try {
    throwable if (options->scheme) {
      do_once {
        curl_url_set_or_die(h, CURLUPART_SCHEME, options->scheme, 0);
      }
      erase(options->scheme);
    } else {
      with (curl_char *c_scheme,
            curl_url_get_or_die(h, CURLUPART_SCHEME, &c_scheme, 0),
            curl_free(c_scheme)) {
        options->scheme = strdup(c_scheme);
      }
    }

    if (options->port) {
      do_once {
        curl_url_set_or_die(h, CURLUPART_PORT, options->port, 0);
      }
      erase(options->port);
      check;
    }

    if (options->username == NULL) {
      throwable try {
        with (curl_char *c_username,
              curl_url_get_or_die(h, CURLUPART_USER, &c_username, 0),
              curl_free(c_username)) {
          options->username = strdup(c_username);
          curl_url_set_or_die(h, CURLUPART_USER, NULL, 0);
        }
      } catch (CurlUrlException, e) {
        if (e->code != CURLUE_NO_USER) {
          throw;
        }
      } onerror (e) { }
    }

    if (options->password == NULL) {
      throwable try {
        with (curl_char *c_password,
              curl_url_get_or_die(h, CURLUPART_PASSWORD, &c_password, 0),
              curl_free(c_password)) {
          options->password = strdup(c_password);
          curl_url_set_or_die(h, CURLUPART_PASSWORD, NULL, 0);
        }
      } catch (CurlUrlException, e) {
        if (e->code != CURLUE_NO_PASSWORD) {
          throw;
        }
      } onerror (e) { }
    }
  } onerror (e) {
    ret = 1;
  }

  return ret;
}


static void replace_password (int argc, char *argv[]) {
  optind = 0;

  bool url_processed = false;

  for (int opt; (opt = getopt(argc, argv, "-:o:")) != -1;) {
    switch (opt) {
      case 1:
        if (url_processed) {
          break;
        }
        url_processed = true;

        char *password_end = strchr(optarg, '@');
        if (password_end == NULL) {
          break;
        }

        *password_end = '\0';
        char *password_start = strrchr(optarg, ':');
        *password_end = '@';
        if (password_start == NULL) {
          break;
        }
        password_start++;

        memset(password_start, '*', password_end - password_start);
        break;

      case 'o':
        for (char *subopts = optarg, *subopts_end = optarg + strlen(optarg); *subopts != '\0';) {
          static char * const mount_opts[] = {"password", NULL};
          char *value;

          int suboptind = getsubopt(&subopts, mount_opts, &value);
          if (subopts < subopts_end) {
            subopts[-1] = ',';
          }
          if (suboptind == 0) {
            memset(value, '*', subopts - value - (subopts < subopts_end));
            break;
          }
        }
        break;
    }
  }

  optind = 0;
}


static void show_version (const char *progname) {
  printf("%s %s libcurl/%s fuse/%u.%u\n",
          progname,
          NETWORKFS_VERSION,
          curl_version_info(CURLVERSION_NOW)->version,
          FUSE_MAJOR_VERSION,
          FUSE_MINOR_VERSION);
}


static int networkfs_opt_proc (void *data, const char *arg, int key,
        struct fuse_args *outargs) {
  struct _networkfs_opts *opts = data;

  switch (key) {
    case KEY_VERSION:
      show_version(outargs->argv[0]);
      exit(0);
    case FUSE_OPT_KEY_NONOPT:
      if (opts->baseurl == NULL) {
        try {
          opts->baseurl = new(CURLU) ();
          should (opts->baseurl) otherwise {
            throw CurlException(0, CURL_INIT, 0);
            break;
          }
          curl_url_set_or_die(opts->baseurl, CURLUPART_URL, arg,
                              CURLU_DEFAULT_SCHEME | CURLU_NON_SUPPORT_SCHEME);
        } onerror (e) {
          curl_url_cleanup(opts->baseurl);
          opts->baseurl = NULL;
          return -1;
        }

        return 0;
      }
      // fall through
    default:
      /* Pass through unknown options */
      return 1;
  }
}


int main (int argc, char *argv[]) {
  int ret;
  struct fuse_args args = FUSE_ARGS_INIT(argc, argv);

  options_set_default();

  try {
    if (fuse_opt_parse(&args, &options, option_spec, networkfs_opt_proc) == -1) {
      throw NetworkFSException("fuse_opt_parse failed");
    }

    struct fuse_operations *networkfs_oper_p = NULL;

    /* When --help is specified, first print our own file-system
       specific help text, then signal fuse_main to show
       additional help (by adding `--help` to the options again)
       without usage: line (by setting argv[0] to the empty
       string) */
    if (options.show_help) {
      usage(argv[0]);
      if (fuse_opt_add_arg(&args, "-h")) {
        throw NetworkFSException("fuse_opt_add_arg help failed");
      }
      args.argv[0][0] = '\0';
      goto end;
    }

    if (options.baseurl == NULL) {
      throw NetworkFSException("no URL specified");
    }
    throwable combine_url(options.baseurl, &options);

    if (options.hide_password && options.password) {
      replace_password(argc, argv);
    }

    if (options.proxyanyauth) {
      options.proxyauth = CURLAUTH_ANY;
    } else {
      if (options.proxybasic) {
        options.proxyauth |= CURLAUTH_BASIC;
      }
      if (options.proxydigest) {
        options.proxyauth |= CURLAUTH_DIGEST;
      }
      if (options.proxyntlm) {
        options.proxyauth |= CURLAUTH_NTLM;
      }
    }

    if (options.scheme[strlen(options.scheme) - 1] == 's') {
      if (options.use_ssl == 0) {
        options.use_ssl = CURLUSESSL_ALL;
      }
      options.scheme[strlen(options.scheme) - 1] = '\0';
    }

    // Initialize curl library before we are a multithreaded program
    curl_global_init(CURL_GLOBAL_ALL);
    struct proto_operations *proto_oper_p;
    throwable proto_oper_p = get_proto_oper(options.scheme, (struct networkfs_opts *) &options);
    networkfs_oper_p = emulate_networkfs_oper(proto_oper_p);

    if (options.set_uid) {
      char opt[sizeof("-ouid=") + 10];
      snprintf(opt, sizeof(opt), "-ouid=%d", options.uid);
      if (fuse_opt_add_arg(&args, opt)) {
        throw NetworkFSException("fuse_opt_add_arg uid failed");
      }
    }
    if (options.set_gid) {
      char opt[sizeof("-ogid=") + 10];
      snprintf(opt, sizeof(opt), "-ogid=%d", options.gid);
      if (fuse_opt_add_arg(&args, opt)) {
        throw NetworkFSException("fuse_opt_add_arg gid failed");
      }
    }
    if (!options.entry_timeout_set) {
      if (fuse_opt_add_arg(&args, "-oentry_timeout=30")) {
        throw NetworkFSException("fuse_opt_add_arg entry_timeout failed");
      }
    }
    if (!options.negative_timeout_set) {
      if (fuse_opt_add_arg(&args, "-onegative_timeout=10")) {
        throw NetworkFSException("fuse_opt_add_arg negative_timeout failed");
      }
    }
    if (!options.attr_timeout_set) {
      if (fuse_opt_add_arg(&args, "-oattr_timeout=30")) {
        throw NetworkFSException("fuse_opt_add_arg attr_timeout failed");
      }
    }

    end:;
    ret = fuse_main(args.argc, args.argv, networkfs_oper_p, NULL);
  } catch (NetworkFSException, e) {
    fprintf(stderr, "error: %s\n", e->what);
    ret = 1;
  } onerror (e) {
    ret = 1;
  }

  fuse_opt_free_args(&args);
  erase(options.scheme);
  erase(options.port);
  delete(CURLU) options.baseurl;
  options.baseurl = NULL;
  erase(options.username);
  erase(options.password);
  erase(options.interface);
  return ret;
}
