#ifndef UTIL_H
#define UTIL_H

#include <stddef.h>

void format_bytes(char *out, size_t size, double bytes);
void format_uptime(char *out, size_t size, long seconds);
void format_clock(char *out, size_t size, long seconds);
int utf8_width(const char *s);

#endif
