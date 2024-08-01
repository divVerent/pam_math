#include "csv.h"

#include <stdio.h>  // for NULL
#include <stdlib.h> // for malloc
#include <string.h> // for strchr, strcpy, strlen, memcpy

#include "helpers.h" // for d0_strndup

void csv_start(char *s, char **buf) {
  *buf = s;
  for (;;) {
    switch (*s) {
    case 0:
    case '\r':
    case '\n':
      *s = 0;
      return;
    }
    ++s;
  }
}

char *csv_read(char **buf) {
  if (*buf == NULL) {
    return NULL;
  }
  switch (**buf) {
  case '"': {
    char *ret = malloc(strlen(*buf) + 1);
    char *retpos = ret;
    for (;;) {
      ++buf;
      char *endptr = strchr(*buf, '"');
      if (endptr == NULL) {
        // Technically invalid CSV.
        strcpy(retpos, *buf);
        *buf = NULL;
        return ret;
      }
      memcpy(retpos, *buf, endptr - *buf);
      retpos += endptr - *buf;
      *retpos = 0;
      *buf = endptr + 1;
      switch (**buf) {
      case 0:
        *buf = NULL;
        return ret;
      case '"':
        *retpos++ = '"';
        continue;
      case ',':
        ++*buf;
        return ret;
      default:
        // Technically invalid CSV.
        strcpy(retpos, *buf);
        *buf = NULL;
        return ret;
      }
    }
  } break;
  default: {
    char *endptr = strchr(*buf, ',');
    if (endptr == NULL) {
      char *ret = d0_strndup(*buf, strlen(*buf));
      *buf = NULL;
      return ret;
    } else {
      char *ret = d0_strndup(*buf, endptr - *buf);
      *buf = endptr + 1;
      return ret;
    }
  }
  }
}
