#ifndef CPU_INFO_H
#define CPU_INFO_H

#define MAX_CORES 256

typedef struct {
    char model[128];
    int cores;
    double usage_percent;
    double core_usage[MAX_CORES];
    int core_id[MAX_CORES];
    double freq_mhz;
    double temp_c;
    unsigned long long delta_total;
} CpuInfo;

int fetch_cpu_static_info(CpuInfo *info);
void cpu_update(CpuInfo *info);

#endif
