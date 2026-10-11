#include "mem_info.h"
#include <stdio.h>
#include <string.h>

int fetch_mem_info(MemoryInfo *info) {
    FILE *file = fopen("/proc/meminfo", "r");
    if (!file) return -1;

    char key[64];
    unsigned long value;
    char unit[16];

    info->total_kb = 0;
    info->free_kb = 0;
    info->available_kb = 0;
    info->buffers_kb = 0;
    info->cached_kb = 0;

    while (fscanf(file, "%63s %lu %15s", key, &value, unit) == 3) {
        if (strcmp(key, "MemTotal:") == 0) info->total_kb = value;
        else if (strcmp(key, "MemFree:") == 0) info->free_kb = value;
        else if (strcmp(key, "MemAvailable:") == 0) info->available_kb = value;
        else if (strcmp(key, "Buffers:") == 0) info->buffers_kb = value;
        else if (strcmp(key, "Cached:") == 0) info->cached_kb = value;
    }

    fclose(file);
    return 0;
}
