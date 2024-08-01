#ifndef HELPERS_H
#define HELPERS_H

#include <stddef.h> // for size_t

char *d0_asprintf(const char *restrict fmt, ...);
void d0_strlcpy(char *dst, const char *src, size_t dst_size);
char *d0_strndup(const char *s, size_t n);

#endif
