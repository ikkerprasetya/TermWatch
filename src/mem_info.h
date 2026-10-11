<<<<<<< HEAD
#ifndef MEM_INFO_H
#define MEM_INFO_H

typedef struct {
    unsigned long total_kb;
    unsigned long free_kb;
    unsigned long available_kb;
    unsigned long buffers_kb;
    unsigned long cached_kb;
} MemoryInfo;

int fetch_mem_info(MemoryInfo *info);

=======
#ifndef MEM_INFO_H
#define MEM_INFO_H

typedef struct {
    unsigned long total_kb;
    unsigned long free_kb;
    unsigned long available_kb;
    unsigned long buffers_kb;
    unsigned long cached_kb;
} MemoryInfo;

int fetch_mem_info(MemoryInfo *info);
void print_mem_info(const MemoryInfo *info);

>>>>>>> 6ec40683499a86f91f1e5cd6917dca80c8d1fee0
#endif