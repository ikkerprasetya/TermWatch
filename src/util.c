#include "util.h"
#include <stdio.h>

void format_bytes(char *out, size_t size, double bytes) {
    static const char units[] = "BKMGTP";
    int u = 0;
    if (bytes < 0.0) bytes = 0.0;
    while (bytes >= 1024.0 && u < 5) {
        bytes /= 1024.0;
        u++;
    }
    if (u == 0) snprintf(out, size, "%.0fB", bytes);
    else if (bytes < 10.0) snprintf(out, size, "%.2f%c", bytes, units[u]);
    else if (bytes < 100.0) snprintf(out, size, "%.1f%c", bytes, units[u]);
    else snprintf(out, size, "%.0f%c", bytes, units[u]);
}

void format_uptime(char *out, size_t size, long seconds) {
    long d = seconds / 86400;
    long h = (seconds % 86400) / 3600;
    long m = (seconds % 3600) / 60;
    long s = seconds % 60;
    if (d > 0) snprintf(out, size, "%ldd %02ldh %02ldm %02lds", d, h, m, s);
    else if (h > 0) snprintf(out, size, "%ldh %02ldm %02lds", h, m, s);
    else snprintf(out, size, "%ldm %02lds", m, s);
}

void format_clock(char *out, size_t size, long seconds) {
    long d = seconds / 86400;
    long h = (seconds % 86400) / 3600;
    long m = (seconds % 3600) / 60;
    long s = seconds % 60;
    if (d > 0) snprintf(out, size, "%ldd %02ld:%02ld:%02ld", d, h, m, s);
    else snprintf(out, size, "%ld:%02ld:%02ld", h, m, s);
}

int utf8_width(const char *s) {
    int width = 0;
    for (; *s; s++) {
        if (((unsigned char)*s & 0xC0) != 0x80) width++;
    }
    return width;
}
