<<<<<<< HEAD
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
=======
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

void print_mem_info(const MemoryInfo *info) {
    double total_gb = (double)info->total_kb / (1024.0 * 1024.0);
    double avail_gb = (double)info->available_kb / (1024.0 * 1024.0);
    double used_gb  = total_gb - avail_gb;
    double usage_pct = (info->total_kb > 0) ? (used_gb / total_gb) * 100.0 : 0.0;

    printf("\033[1;33m=============== MEMORY INFORMATION ================\033[0m\n");
    printf("  Total Memory   : %.2f GB\n", total_gb);
    printf("  Used Memory    : %.2f GB (%.1f%%)\n", used_gb, usage_pct);
    printf("  Available Mem  : %.2f GB\n", avail_gb);
    printf("  Usage Bar      : [");

    int bars = (int)(usage_pct / 5.0);
    for (int i = 0; i < 20; i++) {
        if (i < bars) printf("=");
        else printf(" ");
    }
    printf("]\n\n");
}
>>>>>>> 6ec40683499a86f91f1e5cd6917dca80c8d1fee0
