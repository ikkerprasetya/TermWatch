#ifndef CPU_INFO_H
#define CPU_INFO_H

typedef struct {
    char model[128];
    int cores;
    double usage_percent;
} CpuInfo;

int fetch_cpu_static_info(CpuInfo *info);
double calculate_cpu_usage(void);

#endif