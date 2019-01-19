#ifndef PROTO_DAV_PARSER_H
#define PROTO_DAV_PARSER_H

#include <time.h>

#include <libxml/parser.h>
#include <libxml/tree.h>

#ifndef LIBXML_PUSH_ENABLED
  #error "Library not compiled with push parser support"
#endif


#define xmlStrscmp(str1, str2) xmlStrncmp(str1, str2, strlen((const char *) str2))


time_t xmlParserTimeNode (xmlNode * const node);
size_t curl_parse_xml (char *ptr, size_t size, size_t nmemb, void *data);


#endif /* PROTO_DAV_PARSER_H */
