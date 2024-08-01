#ifndef HELPERS_H
#define HELPERS_H

#include <stddef.h> // for size_t

char *d0_asprintf(const char *restrict fmt, ...);
char *d0_strndup(const char *s, size_t n);

#endif
