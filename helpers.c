#define _POSIX_C_SOURCE 199309L

#include "helpers.h"

#include <limits.h> // for INT_MAX
#include <stdarg.h> // for va_end, va_start, va_list
#include <stdint.h> // for uint32_t
#include <stdio.h>  // for fprintf, stderr, vsnprintf
#include <stdlib.h> // for malloc, free
#include <string.h> // for memcpy, strlen
#include <time.h>   // for time, clock_gettime

#ifdef __linux__
#include <sys/random.h> // for getrandom
#endif

char *d0_asprintf(const char *restrict fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  char tiny[1];
  int n = vsnprintf(tiny, sizeof(tiny), fmt, ap);
  va_end(ap);
  if (n < 0 || n >= INT_MAX) {
    fprintf(stderr, "ERROR: vsnprintf unexpectedly returned %d\n", n);
    return NULL;
  }
  char *buf = malloc(n + 1);
  if (buf == NULL) {
    fprintf(stderr, "ERROR: could not allocate %d bytes\n", n + 1);
    return NULL;
  }
  va_start(ap, fmt);
  int m = vsnprintf(buf, n + 1, fmt, ap);
  va_end(ap);
  if (m != n) {
    fprintf(stderr,
            "ERROR: vsnprintf non-deterministic: returned %d, then %d\n", n, m);
    free(buf);
    return NULL;
  }
  buf[n] = 0;
  return buf;
}

void d0_strlcpy(char *dst, const char *src, size_t dst_size) {
  if (dst_size == 0) {
    return;
  }
  size_t dst_len = dst_size - 1;
  size_t src_len = strlen(src);
  size_t copy_len = (src_len < dst_len) ? src_len : dst_len;
  memcpy(dst, src, copy_len);
  dst[copy_len] = 0;
}

char *d0_strndup(const char *s, size_t n) {
  size_t size = n + 1;
  if (size < n) {
    fprintf(stderr, "ERROR: invalid string length: %d\n", (int)n);
    return NULL;
  }
  char *out = malloc(size);
  if (out == NULL) {
    fprintf(stderr, "ERROR: could not allocate %d bytes\n", (int)(n + 1));
    return NULL;
  }
  d0_strlcpy(out, s, size);
  return out;
}

static int want_init_random = 1;
uint32_t random_seed;
uint32_t random_buf;

void maybe_init_random() {
  int do_init_random = want_init_random;
  want_init_random = 1;
  if (do_init_random) {
#ifdef __linux__
    if (getrandom(&random_seed, sizeof(random_seed), 0) ==
        sizeof(random_seed)) {
      goto got_seed;
    }
    perror("getrandom");
#endif
    random_seed = (uint32_t)time(NULL);
    struct timespec ts;
    if (!clock_gettime(CLOCK_REALTIME, &ts)) {
      random_seed ^= ts.tv_nsec;
    }
  got_seed:;
  }
  random_buf = random_seed;
}

void skip_next_init_random() { want_init_random = 0; }

int randint(int n) {
  // This is POSIX.1-2001's PRNG.
  // Better PRNG welcome.
  random_buf = random_buf * 1103515245 + 12345;
  int r = (random_buf / 65536) % 32768;
  return r % n;
}
