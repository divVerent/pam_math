#include "asprintf.h"

#include <limits.h> // for INT_MAX
#include <stdarg.h> // for va_end, va_start, va_list
#include <stdio.h>  // for fprintf, vsnprintf, stderr
#include <stdlib.h> // for abort, malloc

char *d0_asprintf(const char *restrict fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  char tiny[1];
  int n = vsnprintf(tiny, sizeof(tiny), fmt, ap);
  va_end(ap);
  if (n < 0 || n >= INT_MAX) {
    fprintf(stderr, "FATAL: vsnprintf unexpectedly returned %d\n", n);
    abort();
  }
  char *buf = malloc(n + 1);
  va_start(ap, fmt);
  int m = vsnprintf(buf, n + 1, fmt, ap);
  va_end(ap);
  if (m != n) {
    fprintf(stderr,
            "FATAL: vsnprintf non-deterministic: returned %d, then %d\n", n, m);
    abort();
  }
  buf[n] = 0;
  return buf;
}
