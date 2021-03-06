#define _GNU_SOURCE

#include <string.h>

#include <grammar/try.h>
#include <grammar/malloc.h>
#include <grammar/foreach.h>
#include <utils.h>

#include "parser.h"


static void __attribute__((constructor)) parser_init (void) {
  tzset();
}


time_t xmlParserTimeNode (xmlNodePtr const node) {
  foreach(LinkedList) (attribute, node->properties) {
    if (xmlStrcmp(attribute->name, BAD_CAST "dt") == 0) {
      break_after {
        auto datetime = attribute->children;
        if (datetime && datetime->content &&
            xmlStrscmp(datetime->content, BAD_CAST "dateTime.") == 0) {
          auto format_name = datetime->content + strlen("dateTime.");
          const char *format;
          if (xmlStrscmp(format_name, BAD_CAST "tz") == 0) {
            format = "%FT%TZ";
          } else if (xmlStrscmp(format_name, BAD_CAST "rfc1123") == 0) {
            format = "%a, %d %b %Y %T GMT";
          } else {
            fprintf(stderr, "Unknown time format %s\n", format_name);
            break;
          }

          if likely (node->children && node->children->content) {
            struct tm tm;
            if likely (strptime((const char *) node->children->content, format, &tm)) {
              return mktime(&tm) - timezone;
            }
          }
        }
      }
    }
  }

  return 0;
}


size_t curl_parse_xml (char *ptr, size_t size, size_t nmemb, void *data) {
  if unlikely (data == NULL || nmemb == 0) {
    return nmemb;
  }

  try {
    xmlParserCtxtPtr *ctxt_p = (xmlParserCtxtPtr *) data;
    if unlikely (*ctxt_p == NULL) {
      xmlKeepBlanksDefault(0);
      *ctxt_p = xmlCreatePushParserCtxt(NULL, NULL, ptr, nmemb, NULL);
      condition_throw(*ctxt_p) MallocException("xmlCreatePushParserCtxt");
    } else {
      if unlikely (xmlParseChunk(*ctxt_p, ptr, nmemb, 0)) {
        xmlParserError(*ctxt_p, "xmlParseChunk");
        MallocException("xmlParseChunk");
      }
    }
  } onerror (e) {
    return 0;
  }

  return nmemb;
}
