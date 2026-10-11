#include "cpu_info.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

int fetch_cpu_static_info(CpuInfo *info) {
    FILE *file = fopen("/proc/cpuinfo", "r");
    if (!file) return -1;

    char line[256];
    info->cores = 0;
    info->model[0] = '\0';

    while (fgets(line, sizeof(line), file)) {
        if (strncmp(line, "model name", 10) == 0 && info->model[0] == '\0') {
            char *colon = strchr(line, ':');
            if (colon) {
                colon += 2;
                colon[strcspn(colon, "\r\n")] = '\0';
                strncpy(info->model, colon, sizeof(info->model) - 1);
            }
        } else if (strncmp(line, "processor", 9) == 0) {
            info->cores++;
        }
    }
    fclose(file);
    return 0;
}

double calculate_cpu_usage(void) {
    static unsigned long long prev_user = 0, prev_nice = 0, prev_system = 0, prev_idle = 0;
    static unsigned long long prev_iowait = 0, prev_irq = 0, prev_softirq = 0, prev_steal = 0;
    static int first_run = 1;

    FILE *file = fopen("/proc/stat", "r");
    if (!file) return 0.0;

    unsigned long long user, nice, system, idle, iowait, irq, softirq, steal;
    if (fscanf(file, "cpu %llu %llu %llu %llu %llu %llu %llu %llu",
               &user, &nice, &system, &idle, &iowait, &irq, &softirq, &steal) != 8) {
        fclose(file);
        return 0.0;
    }
    fclose(file);

    if (first_run) {
        prev_user = user; prev_nice = nice; prev_system = system; prev_idle = idle;
        prev_iowait = iowait; prev_irq = irq; prev_softirq = softirq; prev_steal = steal;
        first_run = 0;
        return 0.0;
    }

    unsigned long long prev_idle_all = prev_idle + prev_iowait;
    unsigned long long idle_all = idle + iowait;

    unsigned long long prev_non_idle = prev_user + prev_nice + prev_system + prev_irq + prev_softirq + prev_steal;
    unsigned long long non_idle = user + nice + system + irq + softirq + steal;

    unsigned long long prev_total = prev_idle_all + prev_non_idle;
    unsigned long long total = idle_all + non_idle;

    unsigned long long total_d = total - prev_total;
    unsigned long long idle_d = idle_all - prev_idle_all;

    prev_user = user; prev_nice = nice; prev_system = system; prev_idle = idle;
    prev_iowait = iowait; prev_irq = irq; prev_softirq = softirq; prev_steal = steal;

    if (total_d == 0) return 0.0;
    return ((double)(total_d - idle_d) / (double)total_d) * 100.0;
}
