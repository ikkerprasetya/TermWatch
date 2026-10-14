#ifndef MEM_INFO_H
#define MEM_INFO_H

typedef struct {
    unsigned long total_kb;
    unsigned long free_kb;
    unsigned long available_kb;
    unsigned long buffers_kb;
    unsigned long cached_kb;
    unsigned long swap_total_kb;
    unsigned long swap_free_kb;
} MemoryInfo;

int fetch_mem_info(MemoryInfo *info);

#endif
