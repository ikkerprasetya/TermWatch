#include "mem_info.h"
#include <stdio.h>
#include <string.h>

int fetch_mem_info(MemoryInfo *info) {
    FILE *file = fopen("/proc/meminfo", "r");
    if (!file) return -1;

    memset(info, 0, sizeof(*info));
    int have_available = 0;
    char line[256];

    while (fgets(line, sizeof(line), file)) {
        char key[64];
        unsigned long value;
        if (sscanf(line, "%63[^:]: %lu", key, &value) != 2) continue;
        if (strcmp(key, "MemTotal") == 0) info->total_kb = value;
        else if (strcmp(key, "MemFree") == 0) info->free_kb = value;
        else if (strcmp(key, "MemAvailable") == 0) {
            info->available_kb = value;
            have_available = 1;
        } else if (strcmp(key, "Buffers") == 0) info->buffers_kb = value;
        else if (strcmp(key, "Cached") == 0) info->cached_kb = value;
        else if (strcmp(key, "SwapTotal") == 0) info->swap_total_kb = value;
        else if (strcmp(key, "SwapFree") == 0) info->swap_free_kb = value;
    }
    fclose(file);

    if (!have_available) info->available_kb = info->free_kb + info->buffers_kb + info->cached_kb;
    if (info->available_kb > info->total_kb) info->available_kb = info->total_kb;
    return 0;
}
