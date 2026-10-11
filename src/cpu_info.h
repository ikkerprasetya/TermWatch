<<<<<<< HEAD
#ifndef CPU_INFO_H
#define CPU_INFO_H

typedef struct {
    char model[128];
    int cores;
    double usage_percent;
} CpuInfo;

int fetch_cpu_static_info(CpuInfo *info);
double calculate_cpu_usage(void);

=======
#ifndef CPU_INFO_H
#define CPU_INFO_H

typedef struct {
    char model[128];
    int cores;
    double usage_percent;
} CpuInfo;

int fetch_cpu_static_info(CpuInfo *info);
double calculate_cpu_usage(void);
void print_cpu_info(const CpuInfo *info);

>>>>>>> 6ec40683499a86f91f1e5cd6917dca80c8d1fee0
#endif