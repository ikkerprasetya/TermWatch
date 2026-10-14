#include "cpu_info.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef struct {
    unsigned long long idle;
    unsigned long long total;
} CpuTimes;

static CpuTimes prev_total;
static CpuTimes prev_core[MAX_CORES];
static int prev_count = -1;
static int freq_source = -1;
static int temp_state = 0;
static char temp_path[160];

static int read_long(const char *path, long *value) {
    FILE *file = fopen(path, "r");
    if (!file) return -1;
    int ok = fscanf(file, "%ld", value) == 1;
    fclose(file);
    return ok ? 0 : -1;
}

static int read_text(const char *path, char *buf, size_t size) {
    FILE *file = fopen(path, "r");
    if (!file) return -1;
    if (!fgets(buf, (int)size, file)) {
        fclose(file);
        return -1;
    }
    fclose(file);
    buf[strcspn(buf, "\r\n")] = '\0';
    return 0;
}

static int parse_times(const char *text, CpuTimes *t) {
    unsigned long long v[8] = { 0 };
    int n = sscanf(text, "%llu %llu %llu %llu %llu %llu %llu %llu",
                   &v[0], &v[1], &v[2], &v[3], &v[4], &v[5], &v[6], &v[7]);
    if (n < 4) return -1;
    t->idle = v[3] + v[4];
    t->total = 0;
    for (int i = 0; i < 8; i++) t->total += v[i];
    return 0;
}

static double usage_between(const CpuTimes *a, const CpuTimes *b) {
    if (b->total <= a->total) return 0.0;
    unsigned long long dt = b->total - a->total;
    unsigned long long di = b->idle >= a->idle ? b->idle - a->idle : 0;
    if (di > dt) di = dt;
    return (double)(dt - di) / (double)dt * 100.0;
}

static double sysfs_freq(const CpuInfo *info, int n) {
    double sum = 0.0;
    int count = 0;
    char path[112];
    long khz;
    for (int i = 0; i < n; i++) {
        snprintf(path, sizeof(path), "/sys/devices/system/cpu/cpu%d/cpufreq/scaling_cur_freq", info->core_id[i]);
        if (read_long(path, &khz) == 0) {
            sum += (double)khz;
            count++;
        }
    }
    return count > 0 ? sum / count / 1000.0 : -1.0;
}

static double cpuinfo_freq(void) {
    FILE *file = fopen("/proc/cpuinfo", "r");
    if (!file) return 0.0;
    char line[256];
    double sum = 0.0;
    int count = 0;
    while (fgets(line, sizeof(line), file)) {
        if (strncmp(line, "cpu MHz", 7) == 0) {
            char *colon = strchr(line, ':');
            if (colon) {
                sum += atof(colon + 1);
                count++;
            }
        }
    }
    fclose(file);
    return count > 0 ? sum / count : 0.0;
}

static void update_freq(CpuInfo *info, int n) {
    if (freq_source < 0) {
        double f = sysfs_freq(info, n);
        freq_source = f > 0.0 ? 1 : 2;
        info->freq_mhz = f > 0.0 ? f : cpuinfo_freq();
        return;
    }
    if (freq_source == 1) {
        double f = sysfs_freq(info, n);
        info->freq_mhz = f > 0.0 ? f : 0.0;
    } else {
        info->freq_mhz = cpuinfo_freq();
    }
}

static void locate_temp(void) {
    char path[160];
    char name[64];
    for (int i = 0; i < 16; i++) {
        snprintf(path, sizeof(path), "/sys/class/hwmon/hwmon%d/name", i);
        if (read_text(path, name, sizeof(name)) != 0) continue;
        if (strcmp(name, "coretemp") == 0 || strcmp(name, "k10temp") == 0 ||
            strcmp(name, "zenpower") == 0 || strcmp(name, "cpu_thermal") == 0 ||
            strcmp(name, "cpu-thermal") == 0) {
            snprintf(temp_path, sizeof(temp_path), "/sys/class/hwmon/hwmon%d/temp1_input", i);
            if (access(temp_path, R_OK) == 0) {
                temp_state = 1;
                return;
            }
        }
    }
    for (int i = 0; i < 16; i++) {
        snprintf(path, sizeof(path), "/sys/class/thermal/thermal_zone%d/type", i);
        if (read_text(path, name, sizeof(name)) != 0) continue;
        if (strstr(name, "x86_pkg_temp") || strstr(name, "cpu") || strstr(name, "soc")) {
            snprintf(temp_path, sizeof(temp_path), "/sys/class/thermal/thermal_zone%d/temp", i);
            if (access(temp_path, R_OK) == 0) {
                temp_state = 1;
                return;
            }
        }
    }
    temp_state = -1;
}

static void update_temp(CpuInfo *info) {
    if (temp_state == 0) locate_temp();
    info->temp_c = -1.0;
    if (temp_state == 1) {
        long milli;
        if (read_long(temp_path, &milli) == 0 && milli > 0) info->temp_c = milli / 1000.0;
    }
}

static int field_matches(const char *line, const char *key) {
    size_t len = strlen(key);
    return strncmp(line, key, len) == 0 && (line[len] == ' ' || line[len] == '\t' || line[len] == ':');
}

int fetch_cpu_static_info(CpuInfo *info) {
    memset(info, 0, sizeof(*info));
    info->temp_c = -1.0;
    long online = sysconf(_SC_NPROCESSORS_ONLN);
    info->cores = online > 0 ? (int)online : 1;
    if (info->cores > MAX_CORES) info->cores = MAX_CORES;

    FILE *file = fopen("/proc/cpuinfo", "r");
    if (!file) return -1;

    char line[256];
    char fallback[128] = "";
    while (fgets(line, sizeof(line), file)) {
        char *colon = strchr(line, ':');
        if (!colon) continue;
        colon++;
        while (*colon == ' ' || *colon == '\t') colon++;
        colon[strcspn(colon, "\r\n")] = '\0';
        if (field_matches(line, "model name") && info->model[0] == '\0') {
            snprintf(info->model, sizeof(info->model), "%s", colon);
        } else if ((field_matches(line, "Model") || field_matches(line, "Hardware")) && fallback[0] == '\0') {
            snprintf(fallback, sizeof(fallback), "%s", colon);
        }
    }
    fclose(file);
    if (info->model[0] == '\0') snprintf(info->model, sizeof(info->model), "%s", fallback);
    return 0;
}

void cpu_update(CpuInfo *info) {
    FILE *file = fopen("/proc/stat", "r");
    if (!file) return;

    char line[512];
    CpuTimes total = { 0, 0 };
    CpuTimes cores[MAX_CORES];
    int ids[MAX_CORES];
    int n = 0;
    int have_total = 0;

    while (fgets(line, sizeof(line), file)) {
        if (strncmp(line, "cpu", 3) != 0) break;
        if (line[3] == ' ') {
            have_total = parse_times(line + 4, &total) == 0;
        } else if (isdigit((unsigned char)line[3]) && n < MAX_CORES) {
            char *end;
            long id = strtol(line + 3, &end, 10);
            if (parse_times(end, &cores[n]) == 0) {
                ids[n] = (int)id;
                n++;
            }
        }
    }
    fclose(file);
    if (!have_total) return;

    info->delta_total = 0;
    if (prev_count == n) {
        info->usage_percent = usage_between(&prev_total, &total);
        if (total.total > prev_total.total) info->delta_total = total.total - prev_total.total;
        for (int i = 0; i < n; i++) info->core_usage[i] = usage_between(&prev_core[i], &cores[i]);
    } else {
        info->usage_percent = 0.0;
        for (int i = 0; i < n; i++) info->core_usage[i] = 0.0;
    }

    for (int i = 0; i < n; i++) {
        info->core_id[i] = ids[i];
        prev_core[i] = cores[i];
    }
    prev_total = total;
    prev_count = n;
    info->cores = n > 0 ? n : 1;

    update_freq(info, n);
    update_temp(info);
}
